#!/bin/bash

# check if jq is installed
if ! command -v jq &> /dev/null
then
    echo "jq could not be found"
    exit
fi


baseJSON=$(cat ./port/port.json)

function set_property {
    local key="$1"
    local value="$2"

    baseJSON=$(echo "$baseJSON" | jq --arg key "$key" --arg value "$value" '.attr[$key] = $value')
}

function set_property_from_file {
    local key="$1"
    local file="$2"
    local value

    if [ -f "$file" ]; then
        value=$(cat "$file")
        set_property "$key" "$value"
    else
        echo "File not found: $file"
    fi
}

set_property_from_file "inst" "./port/txt/instructions.txt"

# add also controls to the md version
controls=$(cat "./port/md/controls.md")
instructions=$(cat "./port/md/instructions.md")

combined="$instructions\n$controls"

set_property "inst_md" "$combined"


set_property_from_file "desc" "./port/txt/description.txt"
set_property_from_file "desc_md" "./port/md/description.md"

jq . <<< "$baseJSON"