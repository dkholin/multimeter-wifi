#pragma once
// Called by the parser with a normalized-state JSON string; net layer serves it over /ws and the relay.
void net_start(void);
void net_publish(const char *json);
