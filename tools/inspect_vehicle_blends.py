"""Read authored vehicle sources without executing embedded Blender scripts."""
import bpy, pathlib, json
root=pathlib.Path(__file__).resolve().parents[1]
for path in sorted((root/'assets/models/source/expansion').glob('*.blend')):
    bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False,use_scripts=False)
    rows=[]
    for o in bpy.context.scene.objects:
        if o.type!='MESH':continue
        rows.append(dict(name=o.name,vertices=len(o.data.vertices),dimensions=list(o.dimensions),
            position=list(o.location),materials=[s.material.name if s.material else None for s in o.material_slots],
            modifiers=[(m.name,m.type) for m in o.modifiers]))
    print('VEHICLE '+path.stem+' '+json.dumps(rows))
    print('MATERIALS '+json.dumps([(m.name,list(m.diffuse_color),m.use_nodes,
        [(n.type,n.name) for n in m.node_tree.nodes] if m.use_nodes else []) for m in bpy.data.materials]))
