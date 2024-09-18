# Copyright 2024, Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

import os
import time
import subprocess
import xml.etree.ElementTree as ET
import argparse

def create_empty_dll(dll_name):
    dll_path = os.path.join(os.getcwd(), dll_name)
    if not os.path.exists(dll_path):
        with open(dll_path, 'w') as f:
            pass
        print(f"Empty {dll_name} created in the current directory.")
    return dll_path

def check_path(command_name):
    if subprocess.call(f"where {command_name}", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) != 0:
        print(f"Error: {command_name} not found in the system PATH.")
        exit(1)

def start_procmon(logfile):
    subprocess.Popen(["Procmon.exe", "/BackingFile", logfile, "/AcceptEula", "/Quiet"])
    time.sleep(5)  # Wait for Procmon to start

def run_ispc():
    print("Running ispc.exe...")
    subprocess.Popen(["ispc.exe"])
    time.sleep(5)  # Wait for ispc to run

def export_log_to_xml(logfile, xml_log):
    subprocess.call(["Procmon.exe", "/OpenLog", logfile, "/SaveAs", xml_log])
    time.sleep(5)  # Wait for export to complete

def filter_events(xml_log, dll_name, dll_path):
    filtered_events = []
    tree = ET.parse(xml_log)
    root = tree.getroot()

    for event in root.iter('event'):
        process_name = event.find('Process_Name').text if event.find('Process_Name') is not None else None
        path = event.find('Path').text if event.find('Path') is not None else None

        if (process_name == "ispc.exe" and path and 
            (path.lower().endswith(dll_name) or dll_name in path.lower())):
            filtered_events.append(event)

    return filtered_events, dll_path

def save_filtered_events(filtered_events, filtered_log):
    filtered_tree = ET.Element("ProcessMonitor")
    
    for event in filtered_events:
        event_node = ET.SubElement(filtered_tree, "event")
        for child in event:
            child_node = ET.SubElement(event_node, child.tag)
            child_node.text = child.text

    filtered_tree = ET.ElementTree(filtered_tree)
    filtered_tree.write(filtered_log)
    print(f"Filtered log file created at: {filtered_log}")

def cleanup_temp_files(dll_path, logfile, xml_log):
    if os.path.exists(dll_path):
        os.remove(dll_path)
    if os.path.exists(logfile):
        os.remove(logfile)
    if os.path.exists(xml_log):
        os.remove(xml_log)
    print("Temporary files cleaned up.")

def main():
    parser = argparse.ArgumentParser(description='DLL Injection Checker')
    parser.add_argument('dll_name', type=str, help='Name of the DLL to be injected')
    args = parser.parse_args()

    dll_path = create_empty_dll(args.dll_name)

    check_path("ispc.exe")
    check_path("Procmon.exe")

    logfile = os.path.join(os.getcwd(), "procmon_log.pml")
    xml_log = os.path.join(os.getcwd(), "procmon_log.xml")
    filtered_log = os.path.join(os.getcwd(), "filtered_log.xml")

    start_procmon(logfile)
    run_ispc()
    subprocess.call(["Procmon.exe", "/Terminate"])
    time.sleep(5)  # Wait for Procmon to terminate

    if not os.path.exists(logfile):
        print("Error: Log file was not created.")
        exit(1)

    export_log_to_xml(logfile, xml_log)

    if not os.path.exists(xml_log):
        print("Error: XML log file was not created.")
        exit(1)

    print("Filtering the results...")
    filtered_events, dll_path = filter_events(xml_log, args.dll_name, dll_path)

    dll_loaded_from_current_dir = False  # Initialize the variable
    if not filtered_events:
        print("No relevant events found for ispc.exe loading ", args.dll_name)
    else:
        dll_loaded_from_current_dir = any(event.find('Path').text == dll_path for event in filtered_events)

        if dll_loaded_from_current_dir:
            print("Error: " + args.dll_name + " was loaded from the current directory.")  # Changed to Error
        else:
            print(args.dll_name + " loaded from system directory or other location. Continuing...")

        # Save all filtered events regardless of the loading location
        save_filtered_events(filtered_events, filtered_log)

    cleanup_temp_files(dll_path, logfile, xml_log)

    return 1 if dll_loaded_from_current_dir else 0  # Return 1 if loaded from current dir, else 0

if __name__ == "__main__":
    exit_code = main()
    exit(exit_code)