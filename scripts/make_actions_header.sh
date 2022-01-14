#!/bin/bash

guard="CHOWDER_ACTIONS_H"
protocol_pattern="void protocol_(act|free)_[a-z_]+\\([a-z_\*, ]+\\)"
message_pattern="void \*message_to_packet_[a-z_]+\\([a-z_\*, ]+\\)"
echo -e "#ifndef $guard\n#define $guard"
action_source="$(cat src/actions/* | tr -d '\n\t')"
echo -n $action_source | egrep -o "$protocol_pattern|$message_pattern" | while read -r line; do
	echo "$line;"
done
echo "#endif // $guard"
