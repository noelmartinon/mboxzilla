#!/bin/bash

if [[ $# -ne 2 ]]; then
 echo "Invalid arguments"
 echo "Syntax: $(basename $0) /path/to/source/folder_eml_gz/ /path/to/target/"
 echo "The mbox files are stored in subfolder /path/to/target/EMAILS.sbd"
 exit 1
fi

source=$1
dest=$2
STARTTIME=$(date +%s)
target=$dest/EMAILS.sbd/

# Remove previous target folder
#rm -rf $target/EMAILS.sbd
#rm -f $target/EMAILS

# Total files
total=`find "${source}/" -type f | wc -l`
echo "Total files = $total"

# Create target directories
mkdir -p "${target}/"
rsync -a -f"+ */" -f"- *" "${source}/" "${target}/"

# Create main mbox target file
touch $dest/EMAILS

# Create mbox files
counter=0
find "${source}" -type f -name '*.gz' -print0 | while read -d '' -r gz; do
    filename="${gz%.*}" # Original eml filename (remove extension .gz)
    filename=$(sed "s|${source}|${target}|g" <<< "${filename}") # Target filename
    targetdir="$(dirname "$filename")" # Current mbox target folder
    mboxfile="$targetdir/$(basename "$(dirname "$filename")").mbox" # Mbox name is the same as parent folder $(basename "$(dirname "$gz")")
    date=`LANG=en_us_88591; date` # Date always in english

    echo "From - ${date}" >> "$mboxfile"
    gzip -dc "$gz" >> "$mboxfile"

    ((counter++))
    echo $counter/$total - $gz
done

# Create mozilla sub-directories ".sbd"
find "${target}" -mindepth 1 -type d -print0 | sort -zr | while read -d '' -r targetdir; do
    if [ `find "$targetdir" -mindepth 1 -type d | wc -l` -gt 0 ]; then
         mv "${targetdir}" "${targetdir}.sbd"
    fi
done

# Move mbox to their parent folder and remove empty dir
find "${target}" -type f -execdir mv {} .. \;
find "${target}" -type d -empty -delete

# Rename mbox files
find "${target}" -type f -name '*.mbox' | while read f; do mv -v "$f" "${f%.*}" > /dev/null 2>&1; done

# Summary
ENDTIME=$(date +%s)
echo "It takes $(($ENDTIME - $STARTTIME)) seconds to complete this task."
echo "All mbox files for thunderbird directory tree are stored in ${target}."

