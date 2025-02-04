// Copyright (C) 2002-2007 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_ANIMATED_MESH_SCENE_NODE_H_INCLUDED__
#define __C_ANIMATED_MESH_SCENE_NODE_H_INCLUDED__

#include "IAnimatedMeshSceneNode.h"
#include "IAnimatedMesh.h"

namespace irr
{
namespace scene
{
	class IDummyTransformationSceneNode;

	class CAnimatedMeshSceneNode : public IAnimatedMeshSceneNode
	{
	public:

		//! constructor
		CAnimatedMeshSceneNode(IAnimatedMesh* mesh, ISceneNode* parent, ISceneManager* mgr,	s32 id,
			const core::vector3df& position = core::vector3df(0,0,0),
			const core::vector3df& rotation = core::vector3df(0,0,0),
			const core::vector3df& scale = core::vector3df(1.0f, 1.0f, 1.0f));

		//! destructor
		virtual ~CAnimatedMeshSceneNode();

		//! sets the current frame. from now on the animation is played from this frame.
		virtual void setCurrentFrame(s32 frame);

		//! frame
		virtual void OnRegisterSceneNode();

		//! OnAnimate() is called just before rendering the whole scene.
		virtual void OnAnimate(u32 timeMs);

		//! renders the node.
		virtual void render();

		//! returns the axis aligned bounding box of this node
		virtual const core::aabbox3d<f32>& getBoundingBox() const;

		//! sets the frames between the animation is looped.
		//! the default is 0 - MaximalFrameCount of the mesh.
		virtual bool setFrameLoop(s32 begin, s32 end);

		//! Sets looping mode which is on by default. If set to false,
		//! animations will not be looped.
		virtual void setLoopMode(bool playAnimationLooped);

		//! Sets a callback interface which will be called if an animation
		//! playback has ended. Set this to 0 to disable the callback again.
		virtual void setAnimationEndCallback(IAnimationEndCallBack* callback=0);

		//! sets the speed with witch the animation is played
		virtual void setAnimationSpeed(f32 framesPerSecond);

		//! returns the material based on the zero based index i. To get the amount
		//! of materials used by this scene node, use getMaterialCount().
		//! This function is needed for inserting the node into the scene hirachy on a
		//! optimal position for minimizing renderstate changes, but can also be used
		//! to directly modify the material of a scene node.
		virtual video::SMaterial& getMaterial(u32 i);

		//! returns amount of materials used by this scene node.
		virtual u32 getMaterialCount();

		//! Creates shadow volume scene node as child of this node
		//! and returns a pointer to it.
		virtual IShadowVolumeSceneNode* addShadowVolumeSceneNode(s32 id,
			bool zfailmethod=true, f32 infinity=10000.0f);

		//! Returns a pointer to a child node, which has the same transformation as
		//! the corrsesponding joint, if the mesh in this scene node is a ms3d mesh.
		virtual ISceneNode* getMS3DJointNode(const c8* jointName);

		//! Returns a pointer to a child node, which has the same transformation as
		//! the corrsesponding joint, if the mesh in this scene node is a x mesh.
		virtual ISceneNode* getXJointNode(const c8* jointName);

		//! Returns a pointer to a child node, which has the same transformation as
		//! the corresponding joint, if the mesh in this scene node is a b3d mesh.
		virtual ISceneNode* getB3DJointNode(const c8* jointName);

		//! Removes a child from this scene node.
		//! Implemented here, to be able to remove the shadow properly, if there is one,
		//! or to remove attached childs.
		virtual bool removeChild(ISceneNode* child);

		//! Starts a MD2 animation.
		virtual bool setMD2Animation(EMD2_ANIMATION_TYPE anim);

		//! Starts a special MD2 animation.
		virtual bool setMD2Animation(const c8* animationName);

		//! Returns the current displayed frame number.
		virtual s32 getFrameNr();
		//! Returns the current start frame number.
		virtual s32 getStartFrame();
		//! Returns the current end frame number.
		virtual s32 getEndFrame();

		//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
		/* In this way it is possible to change the materials a mesh causing all mesh scene nodes
		referencing this mesh to change too. */
		virtual void setReadOnlyMaterials(bool readonly);

		//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
		virtual bool isReadOnlyMaterials();

		//! Sets a new mesh
		virtual void setMesh(IAnimatedMesh* mesh);

		//! Returns the current mesh
		virtual IAnimatedMesh* getMesh(void) { return Mesh; }

		//! Writes attributes of the scene node.
		virtual void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options=0);

		//! Reads attributes of the scene node.
		virtual void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options=0);

		//! Returns type of the scene node
		virtual ESCENE_NODE_TYPE getType() { return ESNT_ANIMATED_MESH; }

		// returns the absolute transformation for a special MD3 Tag if the mesh is a md3 mesh,
		// or the absolutetransformation if it's a normal scenenode
		const SMD3QuaterionTag& getAbsoluteTransformation( const core::stringc & tagname);

		//! updates the absolute position based on the relative and the parents position
		virtual void updateAbsolutePosition();

	private:

		u32 buildFrameNr( u32 timeMs);

		core::array<video::SMaterial> Materials;
		core::aabbox3d<f32> Box;
		IAnimatedMesh* Mesh;

		u32 BeginFrameTime;
		s32 StartFrame;
		s32 EndFrame;
		f32 FramesPerSecond;

		u32 CurrentFrameNr;

		bool Looping;
		bool ReadOnlyMaterials;

		IAnimationEndCallBack* LoopCallBack;
		s32 PassCount;

		IShadowVolumeSceneNode* Shadow;

		core::array<IDummyTransformationSceneNode* > JointChildSceneNodes;

		struct SMD3Special
		{
			core::stringc Tagname;
			SMD3QuaterionTagList AbsoluteTagList;
		};
		SMD3Special MD3Special;

	};

} // end namespace scene
} // end namespace irr

#endif

