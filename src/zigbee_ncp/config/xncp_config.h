#ifndef CONFIG_XNCP_CONFIG_H_
#define CONFIG_XNCP_CONFIG_H_

// Table entries are ephemeral and are expected to be populated before a request is sent.
// This is not the size of any source route table! Rather, this controls how many unique
// destinations can be concurrently contacted with source routing enabled.
#define XNCP_MANUAL_SOURCE_ROUTE_TABLE_SIZE  (20)


// Some manufacturers do not write a board or manufacturer name to the NCP.
// Rather than writing the manufacturing tokens within the application, you can instead
// supply overrides that will be preferred to the manufacturing token values.
#define XNCP_MFG_MANUF_NAME  ("")
#define XNCP_MFG_BOARD_NAME  ("")


// Specify a build string that can be read by the host, augmenting its version info
#define XNCP_BUILD_STRING  ("")


// Override the EZSP patch number. The default (0xFF) disables this.
#define XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE  (0xFF)

#endif /* CONFIG_XNCP_CONFIG_H_ */
