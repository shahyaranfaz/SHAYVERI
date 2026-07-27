#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUTS_DIR="${SCRIPT_DIR}/outputs"
ZIPDIR="${OUTPUTS_DIR}/zips"
IDFILE="${ZIPDIR}/.last_id"
ADDR="http://www.theweekinchess.com/zips/"
WOPTS=""
PGNFILE="${OUTPUTS_DIR}/twic.pgn"
TEMPFILE="${OUTPUTS_DIR}/temp.pgn"

mkdir -p "$OUTPUTS_DIR"

if [ ! -d "$ZIPDIR" ]; then
	echo "Creating folder \"${ZIPDIR}/\""
	mkdir -p "$ZIPDIR"
fi
## the first known issue of TWIC
i=210
## a very high number which will not be reached
last_i=$((9999))

if [ "$#" -eq 0 ]; then
    if [ -f "$IDFILE" ]; then
	LAST_ID=$(cat "$IDFILE")
    else
    	LAST_ID=$(cd "$ZIPDIR"; ls * 2>/dev/null | sort | tail -1 || true)
    	LAST_ID=$(basename "$LAST_ID" .zip)
    fi
    if [ -n "$LAST_ID" ]; then
        echo "Last id: ${LAST_ID}"
        i=$((${LAST_ID} + 1))
    fi
else
    if [ "$1" == "--help" ]; then
        echo "Usage: $0 [<start_issue> [<end_issue> [ <pgnfile>] ] ]"
        exit 1
    elif [ "$#" -ge 2 ]; then
        last_i=$(($2))
    fi
    i=$1
fi

if [ "$#" -eq 3 ]; then
    PGNFILE=$3
    TEMPFILE="${PGNFILE}.temp"
    mkdir -p "$(dirname "$PGNFILE")"
    # Explicit ranges are full rebuilds, so never append duplicate games.
    : > "$PGNFILE"
fi

echo -e "Downloading issues \033[31m$i \033[0mto \033[31m${last_i}\033[0m"
echo "Starting download from issue $i..."
DOWNLOADED=""
CONT="true"

while [ "$CONT" == "true" ]; do
    echo -ne "\033[33m downloading TWIC issue \033[31m$i\033[33m...."
    dest_name="twic${i}.zip"
    zip_path="${ZIPDIR}/${dest_name}"

    if [ -s "$zip_path" ] && unzip -tq "$zip_path" >/dev/null 2>&1; then
        echo -e "\033[36m cached"
        DOWNLOADED="${DOWNLOADED} ${dest_name}"
        i=$(($i + 1))
        if [ $i -gt ${last_i} ]; then
            CONT="false"
        fi
        continue
    fi

    rm -f "$zip_path"
    wget -q $WOPTS "$ADDR/twic${i}g.zip" -O - > "$zip_path" || true
    if [ -s "$zip_path" ] && unzip -tq "$zip_path" >/dev/null 2>&1; then
        CONT="true"
        echo -e "\033[32m done!"
        DOWNLOADED="${DOWNLOADED} ${dest_name}"
        i=$(($i+ 1))
    else
        echo -e "\033[31m failed or invalid archive!"
        rm -f "$zip_path"
        echo $((i - 1)) > "$IDFILE"
        CONT="false"
    fi
    if [ $i -gt ${last_i} ]; then
        CONT="false"
    fi
done

echo
echo -e "\033[33m ---------------------------------"
echo
echo -en "\033[31m Updating PGN file \033[32m$PGNFILE\033[31m (filtering for 2600+ rated players)...."

for fname in ${DOWNLOADED}; do
    # Extract to temporary file
    unzip -c "$ZIPDIR/${fname}" > "$TEMPFILE"

    # Filter games where both players are rated 2600+
    awk '
    BEGIN { game = ""; white_elo = 0; black_elo = 0; in_game = 0 }

    /^\[Event / {
        if (in_game && white_elo >= 2600 && black_elo >= 2600) {
            print game
        }
        game = $0 "\n"
        white_elo = 0
        black_elo = 0
        in_game = 1
        next
    }

    /^\[WhiteElo "([0-9]+)"\]/ {
        match($0, /"([0-9]+)"/, arr)
        white_elo = arr[1]
        game = game $0 "\n"
        next
    }

    /^\[BlackElo "([0-9]+)"\]/ {
        match($0, /"([0-9]+)"/, arr)
        black_elo = arr[1]
        game = game $0 "\n"
        next
    }

    in_game { game = game $0 "\n" }

    END {
        if (in_game && white_elo >= 2600 && black_elo >= 2600) {
            print game
        }
    }
    ' "$TEMPFILE" >> "$PGNFILE"

    # Clean up temp file
    rm -f "$TEMPFILE"
done

echo -e "\033[32m done! \033[0m"
echo
echo -e "\033[33m ---------------------------------"
echo
#echo -e "\033[31m Rebuilding SCID DB....\033[33m"
#pgnscid -f ${PGNFILE}
#echo
echo -e "\033[32m done! \033[0m"
echo
echo -e "\033[33m --------------------------------- \033[0m"
echo
