## Description ##
Loads a skeleton and an animation from ozz binary archives. This animation is then played-back and applied to the skeleton.
Animation time and playback speed can be tweaked.

&lt;wiki:gadget url="http://ozz.qualipilote.fr/gadgets/samples/playback/sample\_playback.xml" width="800" height="450" border="1" /&gt;

## Concept ##
This sample loads ozz binary archive file, a skeleton and an animation. Ozz binary files can be produced with ozz command line tools (dae2skel tool ouputs a skeleton from a Collada document, dae2anim an animation), or with ozz serializer (ozz::io::OArchive) from your own application/converter.
At every frame the animation is sampled with ozz::animation::SamplingJob. Sampling local-space output is then converted to model-space matrices for rendering using ozz::animation::LocalToModelJob.

## Sample usage ##
Some parameters can be tuned from sample UI:
  * Play/pause animation.
  * Fix animation time.
  * Set playback speed, which can be negative to go backward.

## Implementation ##
  1. Load animation and skeleton.
    1. Open a ozz::io::OArchive object with a valid ozz::io::Stream as argument. The stream can be a ozz::io::File, or your custom io read capable object that inherits from ozz::io::Stream.
    1. Check that the stream stores the expected object type using ozz::io::OArchive::TestTag() function. Object type is specified as a template argument.
    1. Deserialize the object with >> operator.
  1. Allocates runtime buffers (local-space transforms of type ozz::math::SoaTransform, model-space matrices of type ozz::math::Float4x4) with the number of elements required for your skeleton. Note that local-space transform are Soa objects, meaning that 1 ozz::math::SoaTransform can store multiple (4) joints.
  1. Allocates sampling cache (ozz::animation::SamplingCache) with the number of joints required for your animation. This cache is used to store sampling local data as well as optimising key-frame lookup while reading animation forward.
  1. Sample animation to get local-space transformations using ozz::animation::SamplingJob. This job takes as input the animation, the cache and a time at which the animation should be sampled. Output is the local-space transformation array.
  1. Convert local-space transformations to model-space matrices using ozz::animation::LocalToModelJob. It takes as input the skeleton (to know about joint's hierarchy) and local-space transforms. Output is model-space matrices array.
  1. Model-space matrices array can then be used for rendering (to skin a mesh) or updating the scene graph.