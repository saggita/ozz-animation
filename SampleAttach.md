## Description ##
Demonstrates how to attach an object to an animated skeleton's joint. This feature allows for example to render a sword in the hand of a character. This could also be used to attach an animated skeleting to an joint of an other animated object.

## Concept ##
The idea is to get the joint matix from the model-space matrices array (for the selected "hand" joint), and use this matrix to build transformation matrix of the attached object (the "sword").
Model-space matrices are ouputed by LocalToModelJob. An offset (from the joint) can be applied to the joint transformation by concatenating an offset matrix to joint matrix.

&lt;wiki:gadget url="http://ozz.qualipilote.fr/gadgets/samples/attach/sample\_attach.xml" width="800" height="450" border="1" /&gt;

## Sample usage ##
Some parameters can be tuned from sample UI to move the attached object from a join to an other, or change the offset from that joint:
  * Select the joint where the object must be attached: "Select joint" slider.
  * Offset of the attached object from the joint: "Attachment offset" x/y/z sliders.

## Implementation ##
  1. Load animation and skeleton, sample animation to get local-space transoformations, optionally blend with other animations, and finally convert local-space transformations to model-space matrices. See Playback sample for more details about these steps.
  1. Concatenate object offset transformation (offset relative to the joint) with joint model-space matrix to compute final model-space transformation of the "attached object".
  1. Use resulting transformation matrix to render "attached object" or update the scene graph.