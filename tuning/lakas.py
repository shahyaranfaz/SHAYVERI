#!/usr/bin/env python
"""
Lakas

A game parameter optimizer using nevergrad framework
Distributed master/worker mode via filesystem queue.
"""

__author__ = 'fsmosca'
__script_name__ = 'Lakas'
__version__ = 'v0.42.0-dist'
__credits__ = ['ChrisWhittington', 'Claes1981', 'joergoster', 'Matthies',
               'musketeerchess', 'teytaud', 'thehlopster',
               'tryingsomestuff']

import os
import sys
import argparse
import ast
import copy
import json
import uuid
import time
from dataclasses import dataclass
from collections import OrderedDict, deque
from pathlib import Path
from subprocess import Popen, PIPE
import logging
import platform
import shlex
import traceback
from concurrent.futures import ThreadPoolExecutor, as_completed

import nevergrad as ng
import psutil


# --------------------------
# Hardcoded setup (user req)
# --------------------------
HARDCODE_OPTIMIZER = "cmaes"
HARDCODE_MATCH_MANAGER_PATH = "cutechess-cli"
HARDCODE_BASE_TIME_SEC = 5
HARDCODE_INC_TIME_SEC = 0.05
HARDCODE_GAMES_PER_BUDGET = 200
HARDCODE_ENGINE = "./shaybot"
HARDCODE_OPENING_FILE = "./start_opening/ogpt_chess_startpos.epd"


os_name = platform.system()  # Linux, Windows or ''


log_formatter = logging.Formatter("%(asctime)s | %(levelname)-5.5s | %(message)s")
log_formatter2 = logging.Formatter("%(asctime)s | %(process)6d | %(levelname)-5.5s | %(message)s")


def setup_logger(name, log_file, log_formatter, level=logging.INFO, console=False, mode='w'):
    handler = logging.FileHandler(log_file, mode=mode)
    handler.setFormatter(log_formatter)

    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.addHandler(handler)

    logger.propagate = False

    if console:
        consoleHandler = logging.StreamHandler(sys.stdout)
        consoleHandler.setLevel(logging.DEBUG)
        consoleHandler.setFormatter(log_formatter)
        logger.addHandler(consoleHandler)

    return logger


logger = setup_logger(
    'lakas_logger', 'log_lakas.txt', log_formatter,
    level=logging.INFO, console=True, mode='a')

logger2 = setup_logger(
    'match_logger', 'lakas_match.txt', log_formatter2,
    level=logging.INFO, console=False)


# --------------------------
# Parallel-safe file locking
# --------------------------
class FileLock:
    """
    Cross-platform advisory lock using a separate lock file.
    """
    def __init__(self, lock_path: Path, timeout_sec: float = 600.0, poll_sec: float = 0.1):
        self.lock_path = Path(lock_path)
        self.timeout_sec = timeout_sec
        self.poll_sec = poll_sec
        self._fp = None

    def __enter__(self):
        start = time.time()
        self.lock_path.parent.mkdir(parents=True, exist_ok=True)
        self._fp = open(self.lock_path, "a+")

        while True:
            try:
                if os_name.lower() == "windows":
                    import msvcrt
                    msvcrt.locking(self._fp.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(self._fp.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except Exception:
                if time.time() - start > self.timeout_sec:
                    raise TimeoutError(f"Timeout acquiring lock: {self.lock_path}")
                time.sleep(self.poll_sec)
        return self

    def __exit__(self, exc_type, exc, tb):
        try:
            if self._fp is not None:
                if os_name.lower() == "windows":
                    import msvcrt
                    self._fp.seek(0)
                    msvcrt.locking(self._fp.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(self._fp.fileno(), fcntl.LOCK_UN)
        finally:
            if self._fp is not None:
                self._fp.close()
                self._fp = None


# --------------------------
# Utility from original
# --------------------------
def find_process_id_by_name(process_name):
    process_object = []
    for proc in psutil.process_iter():
        try:
            pinfo = proc.as_dict(attrs=['pid', 'name', 'create_time'])
            if process_name.lower() in pinfo['name'].lower():
                process_object.append(pinfo)
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            pass
    return process_object


def log_cpu(proc_list, msg=''):
    if len(proc_list) < 1:
        return

    num_threads = psutil.cpu_count(logical=True)

    for (p, pid, name) in proc_list:
        mem_mbytes = p.memory_info()[0] / (1024 * 1024)
        if os_name.lower() == 'windows':
            cpu_pct = p.cpu_percent(interval=None) / num_threads
        else:
            cpu_pct = p.cpu_percent(interval=None)
        logger2.info(f'{msg:43s},'
                     f' proc_id: {pid},'
                     f' cpu_usage%: {cpu_pct:0.0f},'
                     f' mem_mb: {mem_mbytes:0.0f},'
                     f' num_threads: {num_threads},'
                     f' proc_name: {name}')


def set_param(input_param):
    new_param = {}
    for k, v in input_param.items():
        if type(v) == list:
            new_param.update({k: v[0]})
        else:
            new_param.update({k: v['init']})
    return new_param


def read_result(line: str, match_manager) -> float:
    if match_manager == 'cutechess':
        num_wins = int(line.split(': ')[1].split(' -')[0])
        num_draws = int(line.split(': ')[1].split('-')[2].strip().split()[0])
        num_games = int(line.split('] ')[1].strip())
        result = (num_wins + num_draws / 2) / num_games
    elif match_manager == 'duel':
        result = float(line.split('[')[1].split(']')[0])
    else:
        logger.exception(f'match manager {match_manager} is not supported.')
        raise
    return result


def get_match_commands(engine_file, test_options, base_options,
                       opening_file, opening_file_format, games, depth,
                       concurrency, base_time_sec, inc_time_sec, match_manager,
                       match_manager_path,
                       variant, cutechess_debug, cutechess_wait,
                       move_time, nodes, protocol, timemargin):
    if match_manager == 'cutechess':
        tour_manager = Path(match_manager_path)
    else:
        tour_manager = match_manager_path

    test_name = 'test'
    base_name = 'base'
    pgn_output = 'nevergrad_games.pgn'

    command = f' -concurrency {concurrency}'
    command += ' -tournament round-robin'

    if variant != 'normal':
        command += f' -variant {variant}'

    if match_manager == 'cutechess':
        command += f' -pgnout {pgn_output} fi'

        if move_time is not None:
            command += f' -each st={move_time}'
        elif nodes is not None:
            command += f' -each tc=inf nodes={nodes}'
        else:
            if base_time_sec is not None and inc_time_sec is not None and depth is not None:
                command += f' -each tc=0/0:{base_time_sec}+{inc_time_sec} depth={depth}'
            elif base_time_sec is not None and inc_time_sec is not None:
                command += f' -each tc=0/0:{base_time_sec}+{inc_time_sec}'
            elif base_time_sec is not None:
                command += f' -each tc=0/0:{base_time_sec}'
            elif inc_time_sec is not None and depth is not None:
                command += f' -each tc=0/0:{0}+{inc_time_sec} depth={depth}'
            elif inc_time_sec is not None:
                command += f' -each tc=0/0:{0}+{inc_time_sec}'
            elif depth is not None:
                command += f' -each tc=inf depth={depth}'

        command += f' -engine cmd={engine_file} name={test_name} timemargin={timemargin} proto={protocol} {test_options}'
        command += f' -engine cmd={engine_file} name={base_name} timemargin={timemargin} proto={protocol} {base_options}'
        command += f' -rounds {games//2} -games 2 -repeat 2'
        command += ' -recover'
        command += f' -wait {cutechess_wait}'
        command += f' -openings file={opening_file} order=random format={opening_file_format}'
        command += ' -resign movecount=6 score=700 twosided=true'
        command += ' -draw movenumber=30 movecount=6 score=1'

        if cutechess_debug:
            command += ' -debug'
    else:
        command += f' -pgnout {pgn_output}'
        if depth is not None:
            command += f' -each tc=inf depth={depth}'
        else:
            command += f' -each tc=0/0:{base_time_sec}+{inc_time_sec}'
        command += f' -engine cmd={engine_file} name={test_name} {test_options}'
        command += f' -engine cmd={engine_file} name={base_name} {base_options}'
        command += f' -rounds {games} -repeat 2'
        command += f' -openings file={opening_file}'
        command += f' -draw movenumber=40 movecount=10 score=0'
        command += f' -resign movecount=6 score=900'

    return tour_manager, command


def engine_match(engine_file, test_options, base_options, opening_file,
                 opening_file_format, games=10, depth=None, concurrency=1,
                 base_time_sec=None, inc_time_sec=None,
                 match_manager='cutechess', match_manager_path=None,
                 variant='normal', cutechess_debug=False,
                 cutechess_wait=5000, move_time=None, nodes=None,
                 protocol='uci',
                 timemargin=50) -> float:
    result = ''

    tour_manager, command = get_match_commands(
        engine_file, test_options, base_options, opening_file,
        opening_file_format, games, depth, concurrency, base_time_sec,
        inc_time_sec, match_manager, match_manager_path, variant, cutechess_debug,
        cutechess_wait, move_time, nodes, protocol, timemargin)

    if os_name.lower() == 'windows':
        process = Popen(str(tour_manager) + command, stdout=PIPE, text=True)
    else:
        process = Popen(shlex.split(str(tour_manager) + command), stdout=PIPE, text=True)

    for eline in iter(process.stdout.readline, ''):
        line = eline.strip()
        logger2.info(line)
        if line.startswith('Score of test vs base'):
            result = read_result(line, match_manager)
            if 'Finished match' in line:
                break

    if result == '':
        raise Exception('Error, there is something wrong with the engine match.')

    return result


def lakas_cmaes(instrum, name, input_data_file, budget=100):
    if input_data_file is not None:
        loaded_optimizer = ng.optimizers.ParametrizedCMA()
        optimizer = loaded_optimizer.load(input_data_file)
        logger.info(f'optimizer: {name}, previous budget: {optimizer.num_ask}\n')
    else:
        logger.info(f'optimizer: {name}\n')
        my_opt = ng.optimizers.ParametrizedCMA()
        optimizer = my_opt(parametrization=instrum, budget=budget)
    return optimizer


# --------------------------
# Distributed queue
# --------------------------
@dataclass
class DistPaths:
    root: Path
    jobs: Path
    working: Path
    results: Path
    state: Path
    lock: Path


def get_shared_root(shared_dir_arg: str | None) -> Path:
    # If not specified, use directory containing this script.
    if shared_dir_arg:
        return Path(shared_dir_arg).resolve()
    return Path(__file__).resolve().parent


def dist_paths(shared_root: Path) -> DistPaths:
    root = shared_root / ".lakas_dist"
    return DistPaths(
        root=root,
        jobs=root / "jobs",
        working=root / "working",
        results=root / "results",
        state=root / "state",
        lock=root / "lock" / "master.lock",
    )


def ensure_dirs(p: DistPaths):
    p.jobs.mkdir(parents=True, exist_ok=True)
    p.working.mkdir(parents=True, exist_ok=True)
    p.results.mkdir(parents=True, exist_ok=True)
    p.state.mkdir(parents=True, exist_ok=True)
    p.lock.parent.mkdir(parents=True, exist_ok=True)


def write_json_atomic(path: Path, obj: dict):
    tmp = path.with_suffix(path.suffix + f".tmp.{os.getpid()}")
    tmp.write_text(json.dumps(obj, sort_keys=True), encoding="utf-8")
    os.replace(tmp, path)


def claim_one_job(p: DistPaths, worker_name: str) -> Path | None:
    # Find any job file; claim with atomic rename
    for job_path in sorted(p.jobs.glob("*.json")):
        claimed = p.working / job_path.name
        try:
            os.replace(job_path, claimed)
            logger.info(f"[{worker_name}] claimed job {claimed.name}")
            return claimed
        except FileNotFoundError:
            continue
        except OSError:
            continue
    return None


def worker_loop(args, p: DistPaths):
    worker_name = args.worker_name or f"worker-{os.getpid()}"
    logger.info(f"[{worker_name}] starting worker loop in shared root: {p.root.parent}")

    # Hardcoded items are used for evaluation
    engine_file = HARDCODE_ENGINE
    match_manager_path = HARDCODE_MATCH_MANAGER_PATH
    base_time_sec = HARDCODE_BASE_TIME_SEC
    inc_time_sec = HARDCODE_INC_TIME_SEC
    games_per_budget = HARDCODE_GAMES_PER_BUDGET
    opening_file = HARDCODE_OPENING_FILE

    # Determine opening format
    opening_file_format = Path(opening_file).suffix[1:]
    if opening_file_format in ("fen", "epd"):
        opening_file_format = "epd"
    elif opening_file_format == "":
        opening_file_format = "pgn"

    max_parallel_jobs = max(1, int(args.num_workers))

    def run_job(job_file: Path) -> Path:
        job = json.loads(job_file.read_text(encoding="utf-8"))
        job_id = job["job_id"]
        test_param = job["test_param"]
        base_param = job["base_param"]

        test_options = " ".join([f"option.{k}={v}" for k, v in test_param.items()]).strip()
        base_options = " ".join([f"option.{k}={v}" for k, v in base_param.items()]).strip()

        try:
            result = engine_match(
                engine_file=engine_file,
                test_options=test_options,
                base_options=base_options,
                opening_file=opening_file,
                opening_file_format=opening_file_format,
                games=games_per_budget,
                depth=job.get("depth"),
                concurrency=job["concurrency"],
                base_time_sec=base_time_sec,
                inc_time_sec=inc_time_sec,
                match_manager=job["match_manager"],
                match_manager_path=match_manager_path,
                variant=job["variant"],
                cutechess_debug=job["cutechess_debug"],
                cutechess_wait=job["cutechess_wait"],
                move_time=job.get("move_time"),
                nodes=job.get("nodes"),
                protocol=job["protocol"],
                timemargin=job["timemargin"],
            )
            loss = 1.0 - float(result)
            out = {"job_id": job_id, "ok": True, "result": float(result), "loss": float(loss)}
        except Exception as e:
            out = {"job_id": job_id, "ok": False, "error": str(e), "traceback": traceback.format_exc()}

        result_path = p.results / f"{job_id}.json"
        write_json_atomic(result_path, out)

        # best effort cleanup working file
        try:
            job_file.unlink()
        except Exception:
            pass

        return result_path

    with ThreadPoolExecutor(max_workers=max_parallel_jobs) as ex:
        inflight = set()
        while True:
            # fill
            while len(inflight) < max_parallel_jobs:
                job_file = claim_one_job(p, worker_name)
                if job_file is None:
                    break
                inflight.add(ex.submit(run_job, job_file))

            done = [f for f in inflight if f.done()]
            for f in done:
                inflight.remove(f)
                try:
                    rp = f.result()
                    logger.info(f"[{worker_name}] wrote result {rp.name}")
                except Exception:
                    logger.exception(f"[{worker_name}] job runner crashed unexpectedly")

            if not inflight:
                time.sleep(0.2)


def master_run(args, p: DistPaths):
    # Only one master should run at a time on shared storage.
    with FileLock(p.lock, timeout_sec=5.0, poll_sec=0.2):
        logger.info(f"[master] acquired master lock: {p.lock}")

        # Parse input params exactly like original.
        input_param = ast.literal_eval(args.input_param)
        input_param = OrderedDict(sorted(input_param.items()))
        init_param = set_param(input_param)

        # Build instrumentation
        arg = {}
        for k, v in input_param.items():
            if type(v) == list:
                arg.update({k: ng.p.Choice(v)})
            else:
                if isinstance(v["init"], int):
                    arg.update({k: ng.p.Scalar(init=v['init'], lower=v['lower'],
                                               upper=v['upper']).set_integer_casting()})
                elif isinstance(v["init"], float):
                    arg.update({k: ng.p.Scalar(init=v['init'], lower=v['lower'],
                                               upper=v['upper'])})

        instrum = ng.p.Instrumentation(**arg)
        if not args.deterministic_function:
            instrum.descriptors.deterministic_function = False

        # Load optimizer if requested
        input_data_file = args.input_data_file
        if input_data_file is not None:
            if not Path(input_data_file).is_file():
                input_data_file = None

        optimizer_name = HARDCODE_OPTIMIZER
        optimizer = lakas_cmaes(instrum, optimizer_name, input_data_file, args.budget)

        # Logger callback: master only writes it (safe)
        nevergrad_logger = ng.callbacks.ParametersLogger(args.optimizer_log_file)
        optimizer.register_callback("tell", nevergrad_logger)

        best_param = {}
        best_loss = None

        # Seed like original (simplified for CMA-ES)
        if optimizer.num_ask < 1:
            best_loss = 0.5
            optimizer.tell(instrum, best_loss)
            recommendation = optimizer.provide_recommendation()
            best_param = recommendation.value[1]
            best_loss = optimizer.current_bests["average"].mean

            if args.output_data_file is not None:
                # master only writes dumps; no need for lock besides master lock, but keep atomic
                tmp = Path(args.output_data_file).with_suffix(Path(args.output_data_file).suffix + f".tmp.{os.getpid()}")
                optimizer.dump(str(tmp))
                os.replace(tmp, args.output_data_file)

        elif input_data_file is not None:
            recommendation = optimizer.provide_recommendation()
            best_param = recommendation.value[1]
            best_loss = optimizer.current_bests["average"].mean

        # Job scheduling
        in_flight = {}  # job_id -> (x, job_path)
        pending_results = set()

        max_outstanding = max(1, int(args.num_workers))
        logger.info(f"[master] max outstanding jobs: {max_outstanding}")

        # Base engine params (init) always, unless use_best_param flag
        use_best_param = args.use_best_param
        best_result_threshold = args.best_result_threshold

        def make_base_param():
            if use_best_param and best_param:
                return copy.deepcopy(best_param)
            return copy.deepcopy(init_param)

        for _ in range(args.budget):
            # keep pipeline filled
            while len(in_flight) < max_outstanding:
                x = optimizer.ask()
                test_param = dict(x.kwargs)

                job_id = str(uuid.uuid4())
                job = {
                    "job_id": job_id,
                    "test_param": test_param,
                    "base_param": make_base_param(),
                    "concurrency": args.concurrency,
                    "depth": args.depth,
                    "nodes": args.nodes,
                    "move_time": int(args.move_time_ms) / 1000 if args.move_time_ms is not None else None,
                    "match_manager": args.match_manager,
                    "variant": args.variant,
                    "cutechess_debug": args.cutechess_debug,
                    "cutechess_wait": args.cutechess_wait,
                    "protocol": args.protocol,
                    "timemargin": args.time_margin,
                }

                job_path = p.jobs / f"{job_id}.json"
                write_json_atomic(job_path, job)
                in_flight[job_id] = (x, job_path)
                pending_results.add(job_id)

            # wait for at least one result
            while True:
                got_any = False
                for job_id in list(pending_results):
                    res_path = p.results / f"{job_id}.json"
                    if not res_path.exists():
                        continue

                    res = json.loads(res_path.read_text(encoding="utf-8"))
                    # cleanup result file after reading (optional)
                    try:
                        res_path.unlink()
                    except Exception:
                        pass

                    x, _job_path = in_flight.pop(job_id)
                    pending_results.remove(job_id)

                    if not res.get("ok", False):
                        logger.error(f"[master] job {job_id} failed: {res.get('error')}")
                        loss = 1.0  # penalize
                    else:
                        loss = float(res["loss"])

                    # Apply best-param dynamics similarly to original Objective when use_best_param enabled
                    if use_best_param:
                        if loss < 1.0 - best_result_threshold:
                            # update best param with this x
                            best_param = copy.deepcopy(dict(x.kwargs))
                        else:
                            # small penalty to avoid oscillation (similar spirit)
                            loss = best_result_threshold + loss * 0.0001

                    optimizer.tell(x, loss)
                    got_any = True

                    if args.output_data_file is not None:
                        tmp = Path(args.output_data_file).with_suffix(Path(args.output_data_file).suffix + f".tmp.{os.getpid()}")
                        optimizer.dump(str(tmp))
                        os.replace(tmp, args.output_data_file)

                    # html plot best-effort (master only)
                    try:
                        exp = nevergrad_logger.to_hiplot_experiment()
                    except ImportError as msg:
                        logger.warning(msg)
                    except Exception:
                        logger.exception('Unexpected exception.')
                    else:
                        exp.to_html(f'{args.optimizer_log_file}.html')

                    break

                if got_any:
                    break
                time.sleep(0.2)

        logger.info("[master] done")


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        prog='%s %s' % (__script_name__, __version__),
        description='Parameter optimizer using nevergrad library.',
        epilog='%(prog)s')

    # distributed
    parser.add_argument('--distributed-role', required=False, type=str,
                        help='Run as master or worker. Default=master.',
                        default='master')
    parser.add_argument('--shared-dir', required=False, type=str,
                        help='Shared directory root for distributed queue. Default=directory containing lakas.py.',
                        default=None)
    parser.add_argument('--worker-name', required=False, type=str,
                        help='Worker name for logging.',
                        default=None)

    # keep existing knobs (do not "fix" these)
    parser.add_argument('--protocol', required=False, default='uci')
    parser.add_argument('--depth', required=False)
    parser.add_argument('--move-time-ms', required=False)
    parser.add_argument('--nodes', required=False)
    parser.add_argument('--time-margin', required=False, default=50)

    parser.add_argument('--budget', required=False, type=int, default=1000)
    parser.add_argument('--concurrency', required=False, type=int, default=1)

    # this one now controls master outstanding jobs OR worker local parallel jobs
    parser.add_argument('--num_workers', required=False, type=int, default=1,
                        help='MASTER: max outstanding eval jobs in flight. WORKER: max parallel jobs on this PC.')

    parser.add_argument('--match-manager', required=False, type=str, default='cutechess')
    parser.add_argument('--variant', required=False, type=str, default='normal')

    parser.add_argument('--input-data-file', required=False, type=str)
    parser.add_argument('--output-data-file', required=False, type=str)
    parser.add_argument('--optimizer-log-file', required=False, type=str, default='log_nevergrad.txt')

    parser.add_argument('--input-param', required=True, type=str)

    parser.add_argument('--common-param', required=False, type=str)
    parser.add_argument('--deterministic-function', action='store_true')
    parser.add_argument('--use-best-param', action='store_true')
    parser.add_argument('--best-result-threshold', required=False, type=float, default=0.5)
    parser.add_argument('--cutechess-debug', action='store_true')
    parser.add_argument('--cutechess-wait', required=False, type=int, default=5000)

    args = parser.parse_args()

    # hardcode per user request
    args.engine = HARDCODE_ENGINE
    args.optimizer = HARDCODE_OPTIMIZER
    args.match_manager_path = HARDCODE_MATCH_MANAGER_PATH
    args.base_time_sec = HARDCODE_BASE_TIME_SEC
    args.inc_time_sec = HARDCODE_INC_TIME_SEC
    args.games_per_budget = HARDCODE_GAMES_PER_BUDGET
    args.opening_file = HARDCODE_OPENING_FILE

    # shared queue paths
    shared_root = get_shared_root(args.shared_dir)
    p = dist_paths(shared_root)
    ensure_dirs(p)

    role = args.distributed_role.lower()
    if role == "worker":
        worker_loop(args, p)
    else:
        master_run(args, p)


if __name__ == "__main__":
    main()