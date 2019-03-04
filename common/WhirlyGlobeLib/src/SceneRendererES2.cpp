/*
 *  SceneRendererES2.mm
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 10/23/12.
 *  Copyright 2011-2019 mousebird consulting
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#import "SceneRendererES2.h"
#import "GLUtils.h"
#import "MaplyView.h"
#import "WhirlyKitLog.h"

using namespace Eigen;
using namespace WhirlyKit;

namespace WhirlyKit
{

// Keep track of a drawable and the MVP we're supposed to use with it
class DrawableContainer
{
public:
    DrawableContainer(Drawable *draw) : drawable(draw) { mvpMat = mvpMat.Identity(); mvMat = mvMat.Identity();  mvNormalMat = mvNormalMat.Identity(); }
    DrawableContainer(Drawable *draw,Matrix4d mvpMat,Matrix4d mvMat,Matrix4d mvNormalMat) : drawable(draw), mvpMat(mvpMat), mvMat(mvMat), mvNormalMat(mvNormalMat) { }
    
    Drawable *drawable;
    Matrix4d mvpMat,mvMat,mvNormalMat;
};

// Alpha stuff goes at the end
// Otherwise sort by draw priority
class DrawListSortStruct2
{
public:
    DrawListSortStruct2(bool useAlpha,bool useZBuffer,RendererFrameInfo *frameInfo) : useAlpha(useAlpha), useZBuffer(useZBuffer), frameInfo(frameInfo)
    {
    }
    DrawListSortStruct2() { }
    DrawListSortStruct2(const DrawListSortStruct2 &that) : useAlpha(that.useAlpha), useZBuffer(that.useZBuffer), frameInfo(that.frameInfo)
    {
    }
    DrawListSortStruct2 & operator = (const DrawListSortStruct2 &that)
    {
        useAlpha = that.useAlpha;
        useZBuffer= that.useZBuffer;
        frameInfo = that.frameInfo;
        return *this;
    }
    bool operator()(const DrawableContainer &conA, const DrawableContainer &conB)
    {
        Drawable *a = conA.drawable;
        Drawable *b = conB.drawable;
        // We may or may not sort all alpha containing drawables to the end
        if (useAlpha)
            if (a->hasAlpha(frameInfo) != b->hasAlpha(frameInfo))
                return !a->hasAlpha(frameInfo);
 
        if (a->getDrawPriority() == b->getDrawPriority())
        {
            if (useZBuffer)
            {
                bool bufferA = a->getRequestZBuffer();
                bool bufferB = b->getRequestZBuffer();
                if (bufferA != bufferB)
                    return !bufferA;
            }
        }
                
        return a->getDrawPriority() < b->getDrawPriority();
    }
    
    bool useAlpha,useZBuffer;
    RendererFrameInfo *frameInfo;
};
    
SceneRendererES2::SceneRendererES2()
    : extraFrameDrawn(false)
{
    // Add a simple default light
    DirectionalLight light;
    light.setPos(Vector3f(0.75, 0.5, -1.0));
    light.setViewDependent(true);
    light.setAmbient(Vector4f(0.6, 0.6, 0.6, 1.0));
    light.setDiffuse(Vector4f(0.5, 0.5, 0.5, 1.0));
    light.setSpecular(Vector4f(0, 0, 0, 0));
    addLight(light);

    lightsLastUpdated = TimeGetCurrent();
}

SceneRendererES2::~SceneRendererES2()
{
}

void SceneRendererES2::forceRenderSetup()
{
    for (auto &renderTarget : renderTargets)
        renderTarget->isSetup = false;
}

void SceneRendererES2::setScene(WhirlyKit::Scene *inScene)
{
    SceneRendererES::setScene(inScene);
    scene = inScene;
}

/// Add a light to the existing set
void SceneRendererES2::addLight(const DirectionalLight &light)
{
    lights.push_back(light);
    lightsLastUpdated = TimeGetCurrent();
    triggerDraw = true;
}

/// Replace all the lights at once. nil turns off lighting
void SceneRendererES2::replaceLights(const std::vector<DirectionalLight> &newLights)
{
    lights.clear();
    for (auto light : newLights)
        lights.push_back(light);
    
    lightsLastUpdated = TimeGetCurrent();
    triggerDraw = true;
}

void SceneRendererES2::setDefaultMaterial(const Material &mat)
{
    defaultMat = mat;
    lightsLastUpdated = TimeGetCurrent();
    triggerDraw = true;
}

void SceneRendererES2::setClearColor(const RGBAColor &color)
{
    if (renderTargets.empty())
        return;
    
    RenderTargetRef defaultTarget = renderTargets.back();
    color.asUnitFloats(defaultTarget->clearColor);
    
    clearColor = color;
    forceRenderSetup();
}
    
void SceneRendererES2::processScene()
{
    if (!scene)
        return;
    
    scene->processChanges(theView,this,TimeGetCurrent());
}
        
bool SceneRendererES2::hasChanges()
{
    return scene->hasChanges(TimeGetCurrent()) || viewDidChange() || !contRenderRequests.empty();
}

void SceneRendererES2::render(TimeInterval duration)
{
    if (!scene)
        return;
    
    frameCount++;
    
    if (framebufferWidth <= 0 || framebufferHeight <= 0)
    {
        // Process the scene even if the window isn't up
        processScene();
        return;
    }

    theView->animate();
    
    TimeInterval now = TimeGetCurrent();

    // Decide if we even need to draw
    if (!hasChanges())
    {
        if (!extraFrameMode)
            return;
        if (extraFrameDrawn)
            return;
        extraFrameDrawn = true;
    } else
        extraFrameDrawn = false;

    lastDraw = now;
        
    if (perfInterval > 0)
        perfTimer.startTiming("Render Frame");
    	
    if (perfInterval > 0)
        perfTimer.startTiming("Render Setup");
    
//    if (!renderSetup)
    {
        // Turn on blending
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }

    // See if we're dealing with a globe or map view
    Maply::MapView *mapView = dynamic_cast<Maply::MapView *>(theView);
    float overlapMarginX = 0.0;
    if (mapView) {
        overlapMarginX = scene->getOverlapMargin();
    }

    // Get the model and view matrices
    Eigen::Matrix4d modelTrans4d = theView->calcModelMatrix();
    Eigen::Matrix4f modelTrans = Matrix4dToMatrix4f(modelTrans4d);
    Eigen::Matrix4d viewTrans4d = theView->calcViewMatrix();
    Eigen::Matrix4f viewTrans = Matrix4dToMatrix4f(viewTrans4d);
    
    // Set up a projection matrix
    Point2f frameSize(framebufferWidth,framebufferHeight);
    Eigen::Matrix4d projMat4d = theView->calcProjectionMatrix(frameSize,0.0);
    
    Eigen::Matrix4f projMat = Matrix4dToMatrix4f(projMat4d);
    Eigen::Matrix4f modelAndViewMat = viewTrans * modelTrans;
    Eigen::Matrix4d modelAndViewMat4d = viewTrans4d * modelTrans4d;
    Eigen::Matrix4d pvMat = projMat4d * viewTrans4d;
    Eigen::Matrix4f mvpMat = projMat * (modelAndViewMat);
    Eigen::Matrix4f mvpNormalMat4f = mvpMat.inverse().transpose();
    Eigen::Matrix4d modelAndViewNormalMat4d = modelAndViewMat4d.inverse().transpose();
    Eigen::Matrix4f modelAndViewNormalMat = Matrix4dToMatrix4f(modelAndViewNormalMat4d);

    switch (zBufferMode)
    {
        case zBufferOn:
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            break;
        case zBufferOff:
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);
            break;
        case zBufferOffDefault:
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_ALWAYS);
            break;
    }
    
//    if (!renderSetup)
    {
        glEnable(GL_CULL_FACE);
        CheckGLError("SceneRendererES2: glEnable(GL_CULL_FACE)");
    }
    
    if (perfInterval > 0)
        perfTimer.stopTiming("Render Setup");
    
	if (scene)
	{
		int numDrawables = 0;
        
        RendererFrameInfo baseFrameInfo;
        baseFrameInfo.glesVersion = glesVersion;
        baseFrameInfo.sceneRenderer = this;
        baseFrameInfo.theView = theView;
        baseFrameInfo.viewTrans = viewTrans;
        baseFrameInfo.viewTrans4d = viewTrans4d;
        baseFrameInfo.modelTrans = modelTrans;
        baseFrameInfo.modelTrans4d = modelTrans4d;
        baseFrameInfo.scene = scene;
        baseFrameInfo.frameLen = duration;
        baseFrameInfo.currentTime = TimeGetCurrent();
        baseFrameInfo.projMat = projMat;
        baseFrameInfo.projMat4d = projMat4d;
        baseFrameInfo.mvpMat = mvpMat;
        Eigen::Matrix4f mvpInvMat = mvpMat.inverse();
        baseFrameInfo.mvpInvMat = mvpInvMat;
        baseFrameInfo.mvpNormalMat = mvpNormalMat4f;
        baseFrameInfo.viewModelNormalMat = modelAndViewNormalMat;
        baseFrameInfo.viewAndModelMat = modelAndViewMat;
        baseFrameInfo.viewAndModelMat4d = modelAndViewMat4d;
        Matrix4f pvMat4f = Matrix4dToMatrix4f(pvMat);
        baseFrameInfo.pvMat = pvMat4f;
        baseFrameInfo.pvMat4d = pvMat;
        theView->getOffsetMatrices(baseFrameInfo.offsetMatrices, frameSize, overlapMarginX);
        Point2d screenSize = theView->screenSizeInDisplayCoords(frameSize);
        baseFrameInfo.screenSizeInDisplayCoords = screenSize;
        baseFrameInfo.lights = &lights;

        // We need a reverse of the eye vector in model space
        // We'll use this to determine what's pointed away
        Eigen::Matrix4f modelTransInv = modelTrans.inverse();
        Vector4f eyeVec4 = modelTransInv * Vector4f(0,0,1,0);
        Vector3f eyeVec3(eyeVec4.x(),eyeVec4.y(),eyeVec4.z());
        baseFrameInfo.eyeVec = eyeVec3;
        Eigen::Matrix4f fullTransInv = modelAndViewMat.inverse();
        Vector4f fullEyeVec4 = fullTransInv * Vector4f(0,0,1,0);
        Vector3f fullEyeVec3(fullEyeVec4.x(),fullEyeVec4.y(),fullEyeVec4.z());
        baseFrameInfo.fullEyeVec = -fullEyeVec3;
        Vector4d eyeVec4d = modelTrans4d.inverse() * Vector4d(0,0,1,0.0);
        baseFrameInfo.heightAboveSurface = 0.0;
        baseFrameInfo.heightAboveSurface = theView->heightAboveSurface();
        baseFrameInfo.eyePos = Vector3d(eyeVec4d.x(),eyeVec4d.y(),eyeVec4d.z()) * (1.0+baseFrameInfo.heightAboveSurface);
        
        if (perfInterval > 0)
            perfTimer.startTiming("Scene preprocessing");

        // Run the preprocess for the changes.  These modify things the active models need.
        int numPreProcessChanges = scene->preProcessChanges(theView, this, now);

        if (perfInterval > 0)
            perfTimer.addCount("Preprocess Changes", numPreProcessChanges);

        if (perfInterval > 0)
            perfTimer.stopTiming("Scene preprocessing");

        if (perfInterval > 0)
            perfTimer.startTiming("Active Model Runs");

        // Let the active models to their thing
        // That thing had better not take too long
        for (auto activeModel : scene->activeModels) {
            activeModel->updateForFrame(&baseFrameInfo);
            // Note: We were setting the GL context here.  Do we need to?
        }
        if (perfInterval > 0)
            perfTimer.addCount("Active Models", (int)scene->activeModels.size());

        if (perfInterval > 0)
            perfTimer.stopTiming("Active Model Runs");

        if (perfInterval > 0)
            perfTimer.addCount("Scene changes", (int)scene->changeRequests.size());
        
        if (perfInterval > 0)
            perfTimer.startTiming("Scene processing");

        // Merge any outstanding changes into the scenegraph
		scene->processChanges(theView,this,now);
        
        if (perfInterval > 0)
            perfTimer.stopTiming("Scene processing");
        
        // Work through the available offset matrices (only 1 if we're not wrapping)
        std::vector<Matrix4d> &offsetMats = baseFrameInfo.offsetMatrices;
        // Turn these drawables in to a vector
        std::vector<DrawableContainer> drawList;
        std::vector<DrawableRef> screenDrawables;
        std::vector<DrawableRef> generatedDrawables;
        std::vector<Matrix4d> mvpMats;
        std::vector<Matrix4d> mvpInvMats;
        std::vector<Matrix4f> mvpMats4f;
        std::vector<Matrix4f> mvpInvMats4f;
        mvpMats.resize(offsetMats.size());
        mvpInvMats.resize(offsetMats.size());
        mvpMats4f.resize(offsetMats.size());
        mvpInvMats4f.resize(offsetMats.size());
        bool calcPassDone = false;
        for (unsigned int off=0;off<offsetMats.size();off++)
        {
            RendererFrameInfo offFrameInfo(baseFrameInfo);
            // Tweak with the appropriate offset matrix
            modelAndViewMat4d = viewTrans4d * offsetMats[off] * modelTrans4d;
            pvMat = projMat4d * viewTrans4d * offsetMats[off];
            modelAndViewMat = Matrix4dToMatrix4f(modelAndViewMat4d);
            mvpMats[off] = projMat4d * modelAndViewMat4d;
            mvpInvMats[off] = (Eigen::Matrix4d)mvpMats[off].inverse();
            mvpMats4f[off] = Matrix4dToMatrix4f(mvpMats[off]);
            mvpInvMats4f[off] = Matrix4dToMatrix4f(mvpInvMats[off]);
            modelAndViewNormalMat4d = modelAndViewMat4d.inverse().transpose();
            modelAndViewNormalMat = Matrix4dToMatrix4f(modelAndViewNormalMat4d);
            Matrix4d &thisMvpMat = mvpMats[off];
            offFrameInfo.mvpMat = mvpMats4f[off];
            offFrameInfo.mvpInvMat = mvpInvMats4f[off];
            mvpNormalMat4f = Matrix4dToMatrix4f(mvpMats[off].inverse().transpose());
            offFrameInfo.mvpNormalMat = mvpNormalMat4f;
            offFrameInfo.viewModelNormalMat = modelAndViewNormalMat;
            offFrameInfo.viewAndModelMat4d = modelAndViewMat4d;
            offFrameInfo.viewAndModelMat = modelAndViewMat;
            Matrix4f pvMat4f = Matrix4dToMatrix4f(pvMat);
            offFrameInfo.pvMat = pvMat4f;
            offFrameInfo.pvMat4d = pvMat;
            
            DrawableRefSet rawDrawables = scene->getDrawables();
            for (DrawableRefSet::iterator it = rawDrawables.begin(); it != rawDrawables.end(); ++it)
            {
                Drawable *theDrawable = it->second.get();
                if (theDrawable->isOn(&offFrameInfo))
                {
                    const Matrix4d *localMat = theDrawable->getMatrix();
                    if (localMat)
                    {
                        Eigen::Matrix4d newMvpMat = projMat4d * viewTrans4d * offsetMats[off] * modelTrans4d * (*localMat);
                        Eigen::Matrix4d newMvMat = viewTrans4d * offsetMats[off] * modelTrans4d * (*localMat);
                        Eigen::Matrix4d newMvNormalMat = newMvMat.inverse().transpose();
                        drawList.push_back(DrawableContainer(theDrawable,newMvpMat,newMvMat,newMvNormalMat));
                    } else
                        drawList.push_back(DrawableContainer(theDrawable,thisMvpMat,modelAndViewMat4d,modelAndViewNormalMat4d));
                }
            }
        }

        // Sort the drawables (possibly multiple of the same if we have offset matrices)
        bool sortLinesToEnd = (zBufferMode == zBufferOffDefault);
        std::sort(drawList.begin(),drawList.end(),DrawListSortStruct2(sortAlphaToEnd,sortLinesToEnd,&baseFrameInfo));
        
        if (perfInterval > 0)
            perfTimer.startTiming("Calculation Shaders");
        
        // Run any calculation shaders
        // These should be independent of screen space, so we only run them once and ignore offsets.
        if (!calcPassDone) {
            // But do we have any
            bool haveCalcShader = false;
            for (unsigned int ii=0;ii<drawList.size();ii++)
                if (drawList[ii].drawable->getCalculationProgram() != EmptyIdentity) {
                    haveCalcShader = true;
                    break;
                }

            if (haveCalcShader) {
                // Have to set an active framebuffer for our empty fragment shaders to write to
                renderTargets[0]->setActiveFramebuffer(this);
                
                glEnable(GL_RASTERIZER_DISCARD);
                
                for (unsigned int ii=0;ii<drawList.size();ii++) {
                    DrawableContainer &drawContain = drawList[ii];
                    SimpleIdentity calcProgID = drawContain.drawable->getCalculationProgram();
                    
                    // Figure out the program to use for drawing
                    if (calcProgID == EmptyIdentity)
                        continue;
                    OpenGLES2Program *program = scene->getProgram(calcProgID);
                    if (program)
                    {
                        glUseProgram(program->getProgram());
                        baseFrameInfo.program = program;
                    }

                    // Tweakers probably not necessary, but who knows
                    drawContain.drawable->runTweakers(&baseFrameInfo);
                    
                    // Run the calculation phase
                    drawContain.drawable->calculate(&baseFrameInfo,scene);
                }

                glDisable(GL_RASTERIZER_DISCARD);
            }
            
            calcPassDone = true;
        }
        
        if (perfInterval > 0)
            perfTimer.stopTiming("Calculation Shaders");

        if (perfInterval > 0)
            perfTimer.startTiming("Draw Execution");
        
        SimpleIdentity curProgramId = EmptyIdentity;
        
        // Iterate through rendering targets here
        for (RenderTargetRef renderTarget : renderTargets)
        {
            renderTarget->setActiveFramebuffer(this);

            if (renderTarget->clearEveryFrame || renderTarget->clearOnce)
            {
                renderTarget->clearOnce = false;
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                CheckGLError("SceneRendererES2: glClear");
            }
            
            bool depthMaskOn = (zBufferMode == zBufferOn);
            for (unsigned int ii=0;ii<drawList.size();ii++)
            {
                DrawableContainer &drawContain = drawList[ii];
                
                // The first time we hit an explicitly alpha drawable
                //  turn off the depth buffer
                if (depthBufferOffForAlpha && !(zBufferMode == zBufferOffDefault))
                {
                    if (depthMaskOn && depthBufferOffForAlpha && drawContain.drawable->hasAlpha(&baseFrameInfo))
                    {
                        depthMaskOn = false;
                        glDisable(GL_DEPTH_TEST);
                    }
                }
                
                // For this mode we turn the z buffer off until we get a request to turn it on
                if (zBufferMode == zBufferOffDefault)
                {
                    if (drawContain.drawable->getRequestZBuffer())
                    {
                        glDepthFunc(GL_LESS);
                        depthMaskOn = true;
                    } else {
                        glDepthFunc(GL_ALWAYS);
                    }
                }
                
                // If we're drawing lines or points we don't want to update the z buffer
                if (zBufferMode != zBufferOff)
                {
                    if (drawContain.drawable->getWriteZbuffer())
                        glDepthMask(GL_TRUE);
                    else
                        glDepthMask(GL_FALSE);
                }

                // Set up transforms to use right now
                Matrix4f currentMvpMat = Matrix4dToMatrix4f(drawContain.mvpMat);
                Matrix4f currentMvpInvMat = Matrix4dToMatrix4f(drawContain.mvpMat.inverse());
                Matrix4f currentMvMat = Matrix4dToMatrix4f(drawContain.mvMat);
                Matrix4f currentMvNormalMat = Matrix4dToMatrix4f(drawContain.mvNormalMat);
                baseFrameInfo.mvpMat = currentMvpMat;
                baseFrameInfo.mvpInvMat = currentMvpInvMat;
                baseFrameInfo.viewAndModelMat = currentMvMat;
                baseFrameInfo.viewModelNormalMat = currentMvNormalMat;
                
                // Figure out the program to use for drawing
                SimpleIdentity drawProgramId = drawContain.drawable->getProgram();
                if (drawProgramId != curProgramId)
                {
                    curProgramId = drawProgramId;
                    OpenGLES2Program *program = scene->getProgram(drawProgramId);
                    if (program)
                    {
                        //                    [renderStateOptimizer setUseProgram:program->getProgram()];
                        glUseProgram(program->getProgram());
                        // Assign the lights if we need to
                        if (program->hasLights() && (lights.size() > 0))
                        program->setLights(lights, lightsLastUpdated, &defaultMat, currentMvpMat);
                        // Explicitly turn the lights on
                        program->setUniform(u_numLightsNameID, (int)lights.size());
                        
                        baseFrameInfo.program = program;
                    }
                }
                if (drawProgramId == EmptyIdentity)
                    continue;
                
                // Only draw drawables that are active for the current render target
                if (drawContain.drawable->getRenderTarget() != renderTarget->getId())
                    continue;
                
                // Run any tweakers right here
                drawContain.drawable->runTweakers(&baseFrameInfo);
                
                // Draw using the given program
                drawContain.drawable->draw(&baseFrameInfo,scene);
                
                // If we had a local matrix, set the frame info back to the general one
    //            if (localMat)
    //                offFrameInfo.mvpMat = mvpMat;
                
                numDrawables++;
            }
        }
        
        if (perfInterval > 0)
        perfTimer.addCount("Drawables drawn", numDrawables);
        
        if (perfInterval > 0)
        perfTimer.stopTiming("Draw Execution");
        
        // Anything generated needs to be cleaned up
        generatedDrawables.clear();
        drawList.clear();
    }

//    if (perfInterval > 0)
//        perfTimer.startTiming("glFinish");

//    glFlush();
//    glFinish();

//    if (perfInterval > 0)
//        perfTimer.stopTiming("glFinish");

    if (perfInterval > 0)
        perfTimer.startTiming("Present Renderbuffer");
    
    // Explicitly discard the depth buffer
    const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_FRAMEBUFFER,1,discards);
    CheckGLError("SceneRendererES2: glDiscardFramebufferEXT");

    // Subclass with do the presentation
    presentRender();

    // Snapshots tend to be platform specific
    snapshotCallback();
    
    if (perfInterval > 0)
        perfTimer.stopTiming("Present Renderbuffer");
    
    if (perfInterval > 0)
        perfTimer.stopTiming("Render Frame");
    
	// Update the frames per sec
	if (perfInterval > 0 && frameCount > perfInterval)
	{
        TimeInterval now = TimeGetCurrent();
		TimeInterval howLong =  now - frameCountStart;;
		framesPerSec = frameCount / howLong;
		frameCountStart = now;
		frameCount = 0;
        
        wkLogLevel(Verbose,"---Rendering Performance---");
        wkLogLevel(Verbose," Frames per sec = %.2f",framesPerSec);
        perfTimer.log();
        perfTimer.clear();
	}
}

}
