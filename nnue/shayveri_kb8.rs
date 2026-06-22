use std::env;

use bullet_lib::{
    game::inputs::{get_num_buckets, ChessBucketsMirrored},
    nn::{
        optimiser::{AdamW, AdamWParams},
    },
    trainer::{
        save::SavedFormat,
        schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
        settings::LocalSettings,
    },
    value::{loader::DirectSequentialDataLoader, ValueTrainerBuilder},
};

fn env_or<T: std::str::FromStr>(key: &str, default: T) -> T {
    env::var(key).ok().and_then(|v| v.parse().ok()).unwrap_or(default)
}

fn main() {
    let hl_size = 256;
    let data_files = env::var("DATA_FILES").expect("DATA_FILES must be set");
    let train_id = env::var("TRAIN_ID").unwrap_or_else(|_| "shayveri_v2.7_kb8".to_string());
    let out_dir = env::var("OUT_DIR").unwrap_or_else(|_| "checkpoints".to_string());
    let resume = env::var("RESUME").unwrap_or_default();

    let initial_lr: f32 = env_or("LR", 0.001);
    let final_lr = initial_lr;
    let superbatches: usize = env_or("EPOCHS", 1);
    let batch_size: usize = env_or("BATCH_SIZE", 16_384);
    let batches_per_superbatch: usize = env_or("BATCHES_PER_SUPERBATCH", 6104);
    let wdl_proportion: f32 = env_or("WDL", 0.1);
    let scale: f32 = env_or("SCALE", 400.0);
    let save_rate: usize = env_or("SAVE_EPOCHS", 1);

    #[rustfmt::skip]
    const BUCKET_LAYOUT: [usize; 32] = [
        0, 1, 2, 3,
        0, 1, 2, 3,
        0, 1, 2, 3,
        0, 1, 2, 3,
        4, 5, 6, 7,
        4, 5, 6, 7,
        4, 5, 6, 7,
        4, 5, 6, 7,
    ];

    const NUM_INPUT_BUCKETS: usize = get_num_buckets(&BUCKET_LAYOUT);

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(BUCKET_LAYOUT))
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(255),
            SavedFormat::id("l0b").round().quantise::<i16>(255),
            SavedFormat::id("l1w").round().quantise::<i16>(255).transpose(),
            SavedFormat::id("l1b").round().quantise::<i32>(255 * 255),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs| {
            let l0 = builder.new_affine("l0", 768 * NUM_INPUT_BUCKETS, hl_size);
            let l1 = builder.new_affine("l1", 2 * hl_size, 1);

            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);

            l1.forward(hidden_layer)
        });

    let stricter_clipping = AdamWParams { max_weight: 0.99, min_weight: -0.99, ..Default::default() };
    trainer.optimiser.set_params_for_weight("l0w", stricter_clipping);

    let schedule = TrainingSchedule {
        net_id: train_id,
        eval_scale: scale,
        steps: TrainingSteps {
            batch_size,
            batches_per_superbatch,
            start_superbatch: 1,
            end_superbatch: superbatches,
        },
        wdl_scheduler: wdl::ConstantWDL { value: wdl_proportion },
        lr_scheduler: lr::CosineDecayLR { initial_lr, final_lr, final_superbatch: superbatches },
        save_rate,
    };

    let settings = LocalSettings { threads: 2, test_set: None, output_directory: &out_dir, batch_queue_size: 32 };

    let data_paths: Vec<String> = data_files.split(':').map(str::to_string).collect();
    let data_refs: Vec<&str> = data_paths.iter().map(String::as_str).collect();
    let dataloader = DirectSequentialDataLoader::new(&data_refs);

    if !resume.is_empty() {
        println!("Loading checkpoint: {resume}");
        trainer.load_from_checkpoint(&resume);
    }

    trainer.run(&schedule, &settings, &dataloader);
}
