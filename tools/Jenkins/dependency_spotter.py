import argparse
import re
import urllib.request


def dependency_spotter(os_name: str, first_build: int, second_build: int, pipeline: str, log_file: str):
    if log_file:
        compare_dependencies_for_file(os_name, first_build, second_build, pipeline, log_file)
        return

    first_build_log_files = extract_available_log_files(os_name, first_build, pipeline)
    second_build_log_files = extract_available_log_files(os_name, second_build, pipeline)

    files_in_both_builds = []
    files_missing_from_second_build = []
    for file in first_build_log_files:
        list_to_append = files_in_both_builds if file in second_build_log_files else files_missing_from_second_build
        list_to_append.append(file)

    print("Files available for comparison:")
    if len(files_in_both_builds) > 0:
        for file in files_in_both_builds:
            print(file)
    else:
        print("None")
    print("")
    print("Files missing from second build:")
    if len(files_missing_from_second_build) > 0:
        for file in files_missing_from_second_build:
            print(file)
    else:
        print("None")
    print("")

    if len(files_in_both_builds) == 0:
        return

    print("")
    for file in files_in_both_builds:
        print(f"{file}:")
        print("")
        compare_dependencies_for_file(os_name, first_build, second_build, pipeline, file)
        print("")


def extract_available_log_files(os_name: str, build_number: int, pipeline: str) -> list:
    url = form_url_for_build_artifact(build_number, os_name, pipeline, "")
    build_log_files = []
    regex_logfile = r"(mantid(docs)?[\w]+environment\.txt)"
    # Log files for first build
    with urllib.request.urlopen(url) as file:
        for line in file.readlines():
            regex_result = re.search(pattern=regex_logfile, string=line.decode("utf-8"))
            if regex_result is not None and len(regex_result.groups()) > 0:
                build_log_files.append(str(regex_result.group(0)))
    return build_log_files


def compare_dependencies_for_file(os_name: str, first_build: int, second_build: int, pipeline: str, log_file: str):
    # Form URLs for each build artifact file
    first_build_output_path = form_url_for_build_artifact(first_build, os_name, pipeline, log_file)
    second_build_output_path = form_url_for_build_artifact(second_build, os_name, pipeline, log_file)

    # Read in the packages used, with versions
    first_output_packages = extract_package_versions(first_build_output_path, os_name)
    second_output_packages = extract_package_versions(second_build_output_path, os_name)

    # Next we compare the versions of each package, or say if a package has been added/removed
    packages_added = []
    packages_removed = []
    packages_changed = {}
    for package in first_output_packages:
        if second_output_packages[package] is None:
            packages_removed.extend(package)
        elif second_output_packages[package] != first_output_packages[package]:
            packages_changed[package] = first_output_packages[package] + "  ->  " + second_output_packages[package]
    for package in second_output_packages:
        if first_output_packages[package] is None:
            packages_added.extend(package)

    # Output
    if len(packages_added) > 0:
        print("Packages added:")
        for p in packages_added:
            print(p)
        print("")
    if len(packages_removed) > 0:
        print("Packages removed:")
        for p in packages_removed:
            print(p)
        print("")
    if len(packages_changed) > 0:
        print("Packages changed:")
        for p in packages_changed:
            print(p + " changed from " + packages_changed[p])


def form_url_for_build_artifact(build_number: int, os_name: str, pipeline: str, log_file: str):
    return f"https://builds.mantidproject.org/job/{pipeline}/{build_number}/artifact/conda-bld/{os_name}/env_logs/{log_file}"


def extract_package_versions(url: str, os_name: str) -> dict:
    regex_pattern = os_name + r"\/([\w\-]+)-([\w\-.]+)\.(conda|tar\.bz2)"
    package_version_dict = {}
    with urllib.request.urlopen(url) as file:
        for line in file.readlines():
            regex_result = re.search(pattern=regex_pattern, string=line.decode("utf-8"))
            if regex_result is not None and len(regex_result.groups()) == 3:
                package_name = regex_result.group(1)
                version = regex_result.group(2)
                package_version_dict[package_name] = version
    return package_version_dict


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for checking dependency changes between two Jenkins builds")
    parser.add_argument("-os", help="Operating system string, e.g. linux-64", default="linux-64", type=str)
    parser.add_argument("--first", "-f", help="First (usually passing) build number", type=int)
    parser.add_argument("--second", "-s", help="Second (usually failing) build number", type=int)
    parser.add_argument("--pipeline", "-p", help="Build pipeline", default="main_nightly_deployment_prototype", type=str)
    parser.add_argument("--logfile", "-l", help="Log file name to compare, by default all of them are compared", default="", type=str)
    args = parser.parse_args()
    dependency_spotter(os_name=args.os, first_build=args.first, second_build=args.second, pipeline=args.pipeline, log_file=args.logfile)
