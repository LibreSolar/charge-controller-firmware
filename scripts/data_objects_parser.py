#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Parser to create JSON file containing data object metadata
#
# TODO
# - Parse nesting in .pub/serial/Enable properly (needs to take parent IDs into account)
# - Also add metadata to groups (using "self" key)

import json

output_path = "cc-05.json"      # version has to match DATA_OBJECTS_VERSION

data_objects = {}
id_macros = {}

# get IDs which are defined as macros
with open("src/data_objects.h", 'r') as fd:
    for (num, line) in enumerate(fd, 1):
        if "#define ID_" in line:
            id_macros[line.split()[1]] = int(line.split()[2], base=16)
            continue

# get actual metadata from data_objects file
with open("src/data_objects.cpp", 'r') as fd:
    json_str = ""
    group_name = ""
    save_lines = False
    save_name = False
    beginning_line = 0
    for (num, line) in enumerate(fd, 1):
        if "TS_GROUP" in line:
            group_name = line.strip().split(",")[1].strip(' "')
            data_objects[group_name] = {}
            continue

        if (line.strip(" \n") == "/*{"):
            save_lines = True
            beginning_line = num
            json_str += "{"
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
                data_objects[group_name][name] = {}
                obj_id = line.split("(")[1].split(",")[0]
                try:
                    data_objects[group_name][name]["id"] = int(obj_id, base=16)
                    data_objects[group_name][name]["idx"] = str(obj_id)
                except ValueError:
                    data_objects[group_name][name]["id"] = id_macros[obj_id]
                    data_objects[group_name][name]["idx"] = hex(id_macros[obj_id])
                data_objects[group_name][name].update(json.loads(json_str))
            except Exception as e:
                print("Error while parsing block between lines {} and {}".format(beginning_line, num))
                print(e)
                exit(-1)
            save_name = False
            json_str = ""

# add missing fields
for category in data_objects:
    for item in data_objects[category]:
        if not 'unit' in data_objects[category][item]:
            data_objects[category][item]['unit'] = None
        if not 'min' in data_objects[category][item]:
            data_objects[category][item]['min'] = None
        if not 'max' in data_objects[category][item]:
            data_objects[category][item]['max'] = None

# save result
with open(output_path, "w", encoding='utf8') as filename:
    json.dump(data_objects, filename, indent=4, ensure_ascii=False)

print("Saved JSON output as: " + output_path)
