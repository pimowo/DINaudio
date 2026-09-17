# MQTT

Status: planowane.

Docelowo:
- root `dinaudio-XXXXXX`
- `state/...`
- `command/...`
- `event/...`
- `availability`

Zasady:
- state retained QoS1
- command non-retained QoS1
- availability retained QoS1
- event non-retained QoS1

MQTT jest opcjonalne i nie może wpływać na podstawowe działanie urządzenia.
