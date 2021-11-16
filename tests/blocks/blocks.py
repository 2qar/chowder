import json

with open("../../gamedata/blocks.json") as f:
    blocks = json.load(f)

with open("states.txt", "w") as out:
    for block in blocks:
        for state in blocks[block]["states"]:
            if "properties" in state:
                properties = list(state["properties"].keys())
                properties.sort()
                name = block
                if "default" in state:
                    out.write(f"{name} {state['id']}\n")
                for prop in properties:
                    name += f";{prop}={state['properties'][prop]}"
                out.write(f"{name} {state['id']}\n")
            else:
                out.write(f"{block} {state['id']}\n")
                pass
