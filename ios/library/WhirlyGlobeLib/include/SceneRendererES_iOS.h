/*
 *  SceneRenderereES_iOS.h
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 1/28/19.
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

#import <UIKit/UIKit.h>
#import "SceneRendererGLES.h"

/// Fill this in to get a view snapshot on the next draw
@protocol WhirlyKitSnapshot
/// Rerturn the render target to snapshot.  EmptyIdentity for the screen.
- (WhirlyKit::SimpleIdentity)renderTargetID;
/// Called when the snapshot is taken
- (void)snapshotImage:(UIImage *)image;
/// Called with the raw image data
- (void)snapshotData:(NSData *)data;
@end

namespace WhirlyKit {

/** SceneRenderer - iOS version
 
    This allocates and manages the context.
  */
class SceneRendererGLES_iOS : public SceneRendererGLES
{
public:
    /// Create for use on the screen
    SceneRendererGLES_iOS();
    /// Create for rendering to a texture
    SceneRendererGLES_iOS(int width,int height);
    virtual ~SceneRendererGLES_iOS();

    /// Set the current OpenGL ES context if there is one
    void useContext();
    
    /// Return the associated context
    EAGLContext *getContext();
    
    /// Used to attach the storage to a render target
    virtual void defaultTargetInit(RenderTarget *);
    
    /// If this associated with an OpenGL layer, set that up.
    void setLayer(CAEAGLLayer *layer);
    
    /// Present the render buffer
    virtual void presentRender();
    
    /// Run the snapshot logic
    virtual void snapshotCallback();
    
    /// Want a snapshot, set up this delegate
    void setSnapshotDelegate(NSObject<WhirlyKitSnapshot> *);
    
protected:
    CAEAGLLayer * __weak layer;
    EAGLContext *context;
    NSObject<WhirlyKitSnapshot> *snapshotDelegate;
};
    
typedef std::shared_ptr<SceneRendererES_iOS> SceneRendererES_iOSRef;
    
}
