#ifndef CONFIG_XNCP_CONFIG_H_
#define CONFIG_XNCP_CONFIG_H_

// Table entries are ephemeral and are expected to be populated before a request is sent.
// This is not the size of any source route table! Rather, this controls how many unique
// destinations can be concurrently contacted with source routing enabled.
#define MANUAL_SOURCE_ROUTE_TABLE_SIZE  (20)

#endif /* CONFIG_XNCP_CONFIG_H_ */
