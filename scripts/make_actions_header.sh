#!/bin/bash

guard="CHOWDER_ACTIONS_H"
func_pattern="void protocol_(act|free)_[a-z_]+\\([a-z_\*, ]+\\)"
echo -e "#ifndef $guard\n#define $guard"
cat src/actions/* | tr -d '\n\t' | egrep -o "$func_pattern" | while read -r line; do
	echo "$line;"
done
echo "#endif // $guard"
