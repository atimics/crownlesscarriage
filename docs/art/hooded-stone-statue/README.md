# Bowed stone figure

An editable Blender sculpture from the supplied photo, refined through art and anatomy studies. A thin veil rests on the bowed crown and one temple. A small brow gather and a deeper cheek fold lead into the shoulder drape. The face has closed eyes, clear cheeks, soft brows, shaped nostrils, and a slight smile. The hands rest loosely across the chest with tapered fingers, small joints, nail beds, and carved skin creases.

The body has a gentle weight shift and unequal shoulder and elbow heights. Broad chest planes stretch between points of support. Short folds gather inside the elbows, while unequal skirt folds open toward the hem. Thin turned cuffs frame the wrists. The stone has a smooth face and hands, fine grain and small chisel traces within folds, and wear on exposed edges.

The back, full hem, and base are artistic extensions of the front photo. The gentle expression follows the user's request for an angelic face and hands.

Open `hooded_stone_statue.blend` in Blender 5. The `STATUE` collection has separate editable meshes for the veil, gown, neck, collar, face, sleeves, hands, and base. The `STUDIO` collection has the camera, lights, and floor. The cloth modifiers control thickness and light wear. The stone finishes use procedural nodes.

![Front view](hooded_stone_statue.png)

![Three quarter view](hooded_stone_statue_three_quarter.png)

![Face and hands](hooded_stone_statue_detail.png)

![Hands](hooded_stone_statue_hands.png)

Rebuild the model, four views, and OBJ export from the repository root:

```sh
blender --background --python tools/blender/build_hooded_statue.py -- --output /tmp/hooded-statue --samples 48
```

Make a small front view during sculpt review:

```sh
blender --background --python tools/blender/build_hooded_statue.py -- --output /tmp/hooded-statue-preview --samples 16 --preview
```

The builder has separate face, hand, and cloth modules. The output folder contains a compressed Blender file, four PNG previews, and an OBJ mesh with an MTL file. The OBJ contains the sculpture pieces. The Blender file includes the full studio and procedural materials.

Art references used for the refinement:

- [Corradini's Modesty, Sansevero Museum](https://www.museosansevero.it/en/the-chapel-and-the-veiled-christ/the-statues/modesty): fabric contact and thin carved cloth.
- [Dürer's Praying Hands, Albertina](https://sammlungenonline.albertina.at/objects/40233): finger structure and small surface forms.
- [Late Medieval Sculpture, The Met](https://resources.metmuseum.org/resources/metpublications/pdf/Late_Medieval_Sculpture_The_Metropolitan_Museum_of_Art_Bulletin_v_64_no_4_Spring_2007.pdf): varied fold shapes and surface contrast.
- [Sculpture techniques, V&A](https://www.vam.ac.uk/articles/sculpture-techniques): carving traces and varied stone finishes.

Validation: built, rendered, and exported in Blender 5.0.1. The saved file was reopened to check finite mesh data, the camera, and closed surfaces. Front, three quarter, back, face, and hand views were checked. The repository art inventory check and `git diff --check` passed.
