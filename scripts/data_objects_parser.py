#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json

output_path = "info.json"
json_str = ""
main_obj = {}
temp_obj = {}
current_key = ""
name = ""
beginning_line = 0
save_lines = False
save_name = False

def append_stuff(temp_obj):
    if current_key != "":
        main_obj[current_key] = temp_obj.copy()
        temp_obj.clear()

with open("src/data_objects.cpp", 'r') as fd:
    for (num, line) in enumerate(fd, 1):
        if line =="\n":
            continue
        if "TS_NODE_PATH" in line:
            if current_key != "":
               append_stuff(temp_obj)

            current_key = line.strip().split(",")[1].strip(' "')
            continue

        if (line.strip(" \n") == "/*{"):
            save_lines = True
            beginning_line = num
            json_str +="{"
            continue
        elif (line.strip(" \n") == "}*/"):
            json_str += "}"
            save_lines = False
            save_name = True
            continue

        if save_lines:
            json_str += line

        if save_name:
            try:
                name = line.strip().split(",")[1].strip(' "')
                temp_obj[name] = json.loads(json_str)
            except Exception as e:
                print("Error while parsing block between lines {} and {}".format(beginning_line, num))
                print(e)
                exit(-1)
            save_name = False
            json_str = ""

append_stuff(temp_obj)

# add missing fields
for category in main_obj:
    for node in main_obj[category]:
        if not 'unit' in main_obj[category][node]:
            main_obj[category][node]['unit'] = None
        if not 'min' in main_obj[category][node]:
            main_obj[category][node]['min'] = None
        if not 'max' in main_obj[category][node]:
            main_obj[category][node]['max'] = None

with open(output_path, "w", encoding='utf8') as filename:
    json.dump(main_obj, filename, indent=4, ensure_ascii=False)

print("Saved file under: " + output_path)