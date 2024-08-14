import re
import sys
import json
import pathlib
import xml.etree.ElementTree as ET


NEWLINE_SENTINEL = "7200872e315d2518866d8a02e8258034"


def json_dumps(obj: dict | list) -> str:
    """Compactly dump JSON into a string."""
    return json.dumps(obj, separators=(", ", ": "), indent=4)


cproject_path = pathlib.Path(sys.argv[1])
cproject = cproject_path.read_text()

# Capture all preprocessing directives verbatim
match = re.search(r"(<\?.*\?>)", cproject[:200], flags=re.DOTALL)
processing_instructions = match.group(0)

tree = ET.fromstring(cproject)

for storage_module in tree.findall(".//storageModule"):
    if "projectCommon.copiedFiles" in storage_module.attrib:
        copied_files = json.loads(storage_module.attrib["projectCommon.copiedFiles"])
        copied_files.sort(
            key=lambda f: (f["generated"], f["projectPath"], f["version"])
        )
        storage_module.attrib["projectCommon.copiedFiles"] = json_dumps(copied_files)

    if "cppBuildConfig.projectBuiltInState" in storage_module.attrib:
        project_built_in_state = json.loads(
            storage_module.attrib["cppBuildConfig.projectBuiltInState"]
        )

        for state in project_built_in_state:
            if "resolvedOptionsStr" in state:
                resolved_options = json.loads(state["resolvedOptionsStr"])
                resolved_options.sort(key=lambda o: o["optionId"])

                state["resolvedOptionsStr"] = json_dumps(resolved_options)

            if "builtinIncludesStr" in state:
                state["builtinIncludesStr"] = NEWLINE_SENTINEL.join(
                    state["builtinIncludesStr"].split()
                )

        storage_module.attrib["cppBuildConfig.projectBuiltInState"] = json_dumps(
            project_built_in_state
        )

    if "projectCommon.referencedModules" in storage_module.attrib:
        referenced_modules = json.loads(
            storage_module.attrib["projectCommon.referencedModules"]
        )
        storage_module.attrib["projectCommon.referencedModules"] = json_dumps(
            referenced_modules
        )

# Normalize self-closing tag spacing
xml_text = ET.tostring(tree, encoding="unicode", xml_declaration=False)
xml_text = xml_text.replace(" />", "/>")

# Replace newlines with literals
xml_text = xml_text.replace("&#10;", "\n")
xml_text = xml_text.replace(NEWLINE_SENTINEL, "\n")
xml_text = re.sub(r"\s*\\n\s*", "\n\\n", xml_text, flags=re.MULTILINE)

# Only touch the filesystem if we need to
if processing_instructions + xml_text != cproject:
    cproject_path.write_text(processing_instructions + xml_text)
