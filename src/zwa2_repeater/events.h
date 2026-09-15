/**
 * @file
 *
 * Definitions of events for the repeater application.
 *
 * @copyright 2020 Silicon Laboratories Inc.
 */
#ifndef APPS_REPEATER_EVENTS_H_
#define APPS_REPEATER_EVENTS_H_

#include <ev_man.h>

/**
 * Defines events for the application.
 *
 * These events are not referred to anywhere else than in the application. Hence, they can be
 * altered to suit the required application flow.
 *
 * The events are located in a separate file to make it possible to include them in other
 * application files. An example could be a peripheral driver that enqueues an event when something
 * specific happens.
 */
typedef enum EVENT_APP_REPEATER
{
  EVENT_EMPTY = DEFINE_EVENT_APP_NBR,
  EVENT_APP_BOOTLOADER,
}
EVENT_APP;

#endif /* APPS_REPEATER_EVENTS_H_ */
