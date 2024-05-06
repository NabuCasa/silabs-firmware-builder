import re
import sys
import json
import pathlib
import xml.etree.ElementTree as ET

cproject_path = pathlib.Path(sys.argv[1])
cproject = cproject_path.read_text()

tree = ET.fromstring(cproject)

for storage_module in tree.findall(".//storageModule"):
    if "projectCommon.copiedFiles" in storage_module.attrib:
        copied_files = json.loads(storage_module.attrib["projectCommon.copiedFiles"])
        copied_files.sort(
            key=lambda f: (f["generated"], f["projectPath"], f["version"])
        )
        storage_module.attrib["projectCommon.copiedFiles"] = json.dumps(
            copied_files, separators=(",", ":"), indent=None
        )

    if "cppBuildConfig.projectBuiltInState" in storage_module.attrib:
        project_built_in_state = json.loads(
            storage_module.attrib["cppBuildConfig.projectBuiltInState"]
        )

match = re.search(r'(<\?.*\?>)', cproject[:200], flags=re.DOTALL)
processing_instructions = match.group(0)

xml_data = ET.tostring(tree, encoding="unicode", xml_declaration=False).replace(" />", "/>")
cproject_path.write_text(processing_instructions + xml_data)
