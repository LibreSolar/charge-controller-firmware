#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Parser to create JSON file containing data object metadata
#
# TODO
# - Parse nesting in _pub/serial/Enable properly (needs to take parent IDs into account)
# - Also add metadata to groups (using "self" key)

import json

output_path = "cc-05.json"      # version has to match DATA_OBJECTS_VERSION

data_objects = {}
id_macros = {}

# used to make units provided by ThingSet object names nicer
unit_fix = {
    'degC': '°C',
    'degF': '°F',
    'pct': '%',
}

# get IDs which are defined as macros
with open("app/src/data_objects.h", 'r') as fd:
    for (num, line) in enumerate(fd, 1):
        if "#define ID_" in line:
            id_macros[line.split()[1]] = int(line.split()[2], base=16)
            continue

# get actual metadata from data_objects file
with open("app/src/data_objects.cpp", 'r') as fd:
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
                id_parsed = line.split("(")[1].split(",")[0]

                try:
                    obj_id = int(id_parsed, base=16)
                    obj_idx = str(id_parsed)
                except ValueError:
                    obj_id = id_macros[id_parsed]
                    obj_idx = hex(id_macros[id_parsed])

                if group_name == "":
                    data_objects[name] = {}
                    data_objects[name]["id"] = obj_id
                    data_objects[name]["idx"] = obj_idx
                    data_objects[name].update(json.loads(json_str))
                else:
                    data_objects[group_name][name] = {}
                    data_objects[group_name][name]["id"] = obj_id
                    data_objects[group_name][name]["idx"] = obj_idx
                    data_objects[group_name][name].update(json.loads(json_str))

            except Exception as e:
                print("Error while parsing block between lines {} and {}".format(beginning_line, num))
                print(e)
                exit(-1)

            save_name = False
            json_str = ""

def add_missing_fields(obj, name):
    if not 'unit' in obj:
        chunks = name.split('_')
        if len(chunks) > 2 and name[0] != '_':
            obj['unit'] = chunks[1] + '/' + chunks[2]
        elif len(chunks) > 1 and name[0] != '_':
            obj['unit'] = chunks[1]
        else:
            obj['unit'] = None
        if obj['unit'] in unit_fix:
            obj['unit'] = unit_fix[obj['unit']]
    if not 'min' in obj:
        obj['min'] = None
    if not 'max' in obj:
        obj['max'] = None
    return obj

# add missing fields
for obj_name in data_objects:
    if obj_name[0].islower():
        # data item at root level
        data_objects[obj_name] = add_missing_fields(data_objects[obj_name], obj_name)
    else:
        # group
        for item in data_objects[obj_name]:
            data_objects[obj_name][item] = add_missing_fields(data_objects[obj_name][item], item)

# save result
with open(output_path, "w", encoding='utf8') as filename:
    json.dump(data_objects, filename, indent=4, ensure_ascii=False)

print("Saved JSON output as: " + output_path)
