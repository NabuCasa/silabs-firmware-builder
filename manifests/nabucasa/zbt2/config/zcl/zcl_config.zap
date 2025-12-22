{
  "fileFormat": 2,
  "featureLevel": 109,
  "creator": "zap",
  "keyValuePairs": [
    {
      "key": "commandDiscovery",
      "value": "1"
    },
    {
      "key": "defaultResponsePolicy",
      "value": "always"
    },
    {
      "key": "manufacturerCodes",
      "value": "0x134B"
    }
  ],
  "package": [
    {
      "pathRelativity": "relativeToZap",
      "path": "../../../../../../../Downloads/zap-mac-arm64(2)/zap.app/Contents/Resources/app.asar/zcl-builtin/silabs/zcl.json",
      "type": "zcl-properties",
      "category": "zigbee",
      "version": 1,
      "description": "ZigbeePro test data"
    }
  ],
  "endpointTypes": [
    {
      "id": 1,
      "name": "Anonymous Endpoint Type",
      "deviceTypeRef": {
        "code": 258,
        "profileId": 260,
        "label": "HA-colordimmablelight",
        "name": "HA-colordimmablelight",
        "deviceTypeOrder": 0
      },
      "deviceTypes": [
        {
          "code": 258,
          "profileId": 260,
          "label": "HA-colordimmablelight",
          "name": "HA-colordimmablelight",
          "deviceTypeOrder": 0
        }
      ],
      "deviceVersions": [
        1
      ],
      "deviceIdentifiers": [
        258
      ],
      "deviceTypeName": "HA-colordimmablelight",
      "deviceTypeCode": 258,
      "deviceTypeProfileId": 260,
      "clusters": [
        {
          "name": "Basic",
          "code": 0,
          "mfgCode": null,
          "define": "BASIC_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "ZCL version",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "0x08",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "manufacturer name",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "Nabu Casa",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "model identifier",
              "code": 5,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "ZBT-2 (router)",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "power source",
              "code": 7,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "identify time",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "AddGroup",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddGroupResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroupResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroup",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembershipResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembership",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroup",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroupResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllGroups",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddGroupIfIdentifying",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "name support",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Scenes",
          "code": 5,
          "mfgCode": null,
          "define": "SCENES_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "AddScene",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddSceneResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewScene",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewSceneResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveScene",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveSceneResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllScenes",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllScenesResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "StoreSceneResponse",
              "code": 4,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "StoreScene",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RecallScene",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetSceneMembershipResponse",
              "code": 6,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "GetSceneMembership",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "scene count",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current scene",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current group",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "scene valid",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "name support",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "On/off",
          "code": 6,
          "mfgCode": null,
          "define": "ON_OFF_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "Off",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "On",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Toggle",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "on/off",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 1,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Level Control",
          "code": 8,
          "mfgCode": null,
          "define": "LEVEL_CONTROL_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "MoveToLevel",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Move",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Step",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Stop",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveToLevelWithOnOff",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveWithOnOff",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StepWithOnOff",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StopWithOnOff",
              "code": 7,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "current level",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 1,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "start up current level",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xFF",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Over the Air Bootloading",
          "code": 25,
          "mfgCode": null,
          "define": "OTA_BOOTLOAD_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "ImageNotify",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "QueryNextImageRequest",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "QueryNextImageResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ImageBlockRequest",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ImageBlockResponse",
              "code": 5,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "UpgradeEndRequest",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "UpgradeEndResponse",
              "code": 7,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "OTA Upgrade Server ID",
              "code": 0,
              "mfgCode": null,
              "side": "client",
              "type": "ieee_address",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffffffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Offset (address) into the file",
              "code": 1,
              "mfgCode": null,
              "side": "client",
              "type": "int32u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Current File Version",
              "code": 2,
              "mfgCode": null,
              "side": "client",
              "type": "int32u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Current ZigBee Stack Version",
              "code": 3,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Downloaded File Version",
              "code": 4,
              "mfgCode": null,
              "side": "client",
              "type": "int32u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Upgrade Status",
              "code": 6,
              "mfgCode": null,
              "side": "client",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Manufacturer ID",
              "code": 7,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Image Type ID",
              "code": 8,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "4",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Color Control",
          "code": 768,
          "mfgCode": null,
          "define": "COLOR_CONTROL_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "MoveToColor",
              "code": 7,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveColor",
              "code": 8,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StepColor",
              "code": 9,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveToColorTemperature",
              "code": 10,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveColorTemperature",
              "code": 75,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StepColorTemperature",
              "code": 76,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "remaining time",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current x",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x616B",
              "reportable": 1,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current y",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x607D",
              "reportable": 1,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "color temperature",
              "code": 7,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00FA",
              "reportable": 1,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "color mode",
              "code": 8,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "color control options",
              "code": 15,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "enhanced color mode",
              "code": 16385,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "color temp physical min",
              "code": 16395,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "153",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "color temp physical max",
              "code": 16396,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "625",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "couple color temp to level min-mireds",
              "code": 16397,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "153",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "start up color temperature mireds",
              "code": 16400,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        }
      ]
    }
  ],
  "endpoints": [
    {
      "endpointTypeName": "Anonymous Endpoint Type",
      "endpointTypeIndex": 0,
      "profileId": 260,
      "endpointId": 1,
      "networkId": 0,
      "parentEndpointIdentifier": null
    }
  ]
}