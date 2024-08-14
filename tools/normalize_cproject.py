import re
import sys
import json
import pathlib
import xml.etree.ElementTree as ET


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
        storage_module.attrib["projectCommon.copiedFiles"] = json.dumps(copied_files)

    if "cppBuildConfig.projectBuiltInState" in storage_module.attrib:
        project_built_in_state = json.loads(
            storage_module.attrib["cppBuildConfig.projectBuiltInState"]
        )

        for state in project_built_in_state:
            if "resolvedOptionsStr" in state:
                resolved_options = json.loads(state["resolvedOptionsStr"])
                resolved_options.sort(key=lambda o: o["optionId"])

                state["resolvedOptionsStr"] = json.dumps(resolved_options)

        storage_module.attrib["cppBuildConfig.projectBuiltInState"] = json.dumps(
            project_built_in_state
        )

    if "projectCommon.referencedModules" in storage_module.attrib:
        referenced_modules = json.loads(
            storage_module.attrib["projectCommon.referencedModules"]
        )
        storage_module.attrib["projectCommon.referencedModules"] = json.dumps(
            referenced_modules
        )

# Normalize self-closing tag spacing
xml_text = ET.tostring(tree, encoding="unicode", xml_declaration=False)
xml_text = xml_text.replace(" />", "/>")

# Replace newlines with literals!
xml_text = xml_text.replace("&#10;", "\n")
xml_text = xml_text.replace("\\n", "\n\\n")

# Only touch the filesystem if we need to
if processing_instructions + xml_text != cproject:
    cproject_path.write_text(processing_instructions + xml_text)
