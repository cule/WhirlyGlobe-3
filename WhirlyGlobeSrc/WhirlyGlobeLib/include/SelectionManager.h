/*
 *  SelectionManager.h
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 10/26/11.
 *  Copyright 2011-2013 mousebird consulting. All rights reserved.
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

#import <math.h>
#import <set>
#import <map>
#import "Identifiable.h"
#import "WhirlyGeometry.h"
#import "WhirlyKitView.h"
#import "MaplyView.h"
#import "Scene.h"
#import "ScreenSpaceBuilder.h"

namespace WhirlyKit
{
    
class Scene;
class SceneManager;

/// Base class for selectable geometry
class Selectable
{
public:
    Selectable() : enable(true), minVis(DrawVisibleInvalid), maxVis(DrawVisibleInvalid) { }
    Selectable(SimpleIdentity theID) : selectID(theID), minVis(DrawVisibleInvalid), maxVis(DrawVisibleInvalid) { }
    
    bool enable;
    /// Used to identify this selectable
    SimpleIdentity selectID;
    float minVis,maxVis;  // Range over which this is visible
};

/** This is used internally to the selection layer to track a
    selectable rectangle.  It consists of geometry and an
    ID to track it.
  */
class RectSelectable3D : public Selectable
{
public:    
    RectSelectable3D() : Selectable() { }
    RectSelectable3D(SimpleIdentity theID) : Selectable(theID) { }
    // Comparison operator for sorting
    bool operator < (const RectSelectable3D &that) const;
    
    Point3f pts[4];  // Geometry
    Eigen::Vector3f norm;   // Calculate normal
};

typedef std::set<WhirlyKit::RectSelectable3D> RectSelectable3DSet;

/** This is 3D rectangular solid.
  */
class PolytopeSelectable : public Selectable
{
public:
    PolytopeSelectable() : Selectable() { }
    PolytopeSelectable(SimpleIdentity theID) : Selectable(theID) { }
    // Comparison operator for sorting
    bool operator < (const PolytopeSelectable &that) const;
    
    std::vector<Point3fVector> polys;
    Point3f midPt;        // Point right in the middle of the polytope
};

typedef std::set<WhirlyKit::PolytopeSelectable> PolytopeSelectableSet;

/** This is a linear features with arbitrary 3D points.
  */
class LinearSelectable : public Selectable
{
public:
    LinearSelectable() : Selectable() { }
    LinearSelectable(SimpleIdentity theID) : Selectable(theID) { }
    // Comparison operator for sorting
    bool operator < (const LinearSelectable &that) const;
    
    Point3dVector pts;
};

typedef std::set<WhirlyKit::LinearSelectable> LinearSelectableSet;
    
/** Rectangle Selectable (screen space version).
 */
class RectSelectable2D : public Selectable
{
public:
    RectSelectable2D() : Selectable() { }
    RectSelectable2D(SimpleIdentity theID) : Selectable(theID) { }
    // Comparison operator for sorting
    bool operator < (const RectSelectable2D &that) const;
    
    Point3d center;  // Location of the center of the rectangle
    Point2f pts[4];  // Geometry
};

typedef std::set<WhirlyKit::RectSelectable2D> RectSelectable2DSet;

/// Billboard selectable (3D object that turns towards the viewer)
class BillboardSelectable : public Selectable
{
public:
    BillboardSelectable() : Selectable() { }
    BillboardSelectable(SimpleIdentity theID) : Selectable(theID) { }
    // Comparison operator for sorting
    bool operator < (const BillboardSelectable &that) const;
    
    Point3f center;  // Location of the middle of the base in display space
    Point3f normal;  // The billboard points up in this direction
    Point2f size;    // Size of the billboard in display space
};
  
typedef std::set<WhirlyKit::BillboardSelectable> BillboardSelectableSet;
    
#define kWKSelectionManager "WKSelectionManager"
    
/** The selection manager tracks a variable number of objects that
     might be selectable.  These consist of a shape and an ID.
    Layers (or the caller) can register objects with the
     selection layer.  These objects will be considered for selection
     when the caller uses pickObject.
 
    All objects are currently being projected to the 2D screen and
     evaluated for distance there.
 
    The selection manager is entirely thread safe except for destruction.
 */
class SelectionManager : public SceneManager
{
public:
    /// Pass in the content scaling (not 1.0 if we're on retina)
    SelectionManager(Scene *scene,float viewScale);
    ~SelectionManager();

    /// When we're selecting multiple objects we'll return a list of these
    class SelectedObject
    {
    public:
        SelectedObject(SimpleIdentity selectID,double distIn3D,double screenDist) : selectID(selectID), distIn3D(distIn3D), screenDist(screenDist) { }
        SimpleIdentity selectID;    // What we selected
        double distIn3D;            // 3D distance from eye
        double screenDist;          // 2D distance in screen space
    };

    /// Add a rectangle (in 3-space) for selection
    void addSelectableRect(SimpleIdentity selectId,Point3f *pts,bool enable);
    
    /// Add a rectangle (in 3-space) for selection, but only between the given visibilities
    void addSelectableRect(SimpleIdentity selectId,Point3f *pts,float minVis,float maxVis,bool enable);
    
    /// Add a screen space rectangle (2D) for selection, between the given visibilities
    void addSelectableScreenRect(SimpleIdentity selectId,const Point3d &center,Point2f *pts,float minVis,float maxVis,bool enable);
    
    /// Add a rectangular solid for selection.  Pass in 8 points (bottom four + top four)
    void addSelectableRectSolid(SimpleIdentity selectId,Point3f *pts,float minVis,float maxVis,bool enable);
    
    /// Add a rectangular solid for selection.  Pass in 8 points (bottom four + top four)
    void addSelectableRectSolid(SimpleIdentity selectId,const BBox &bbox,float minVis,float maxVis,bool enable);
    
    /// Add a polytope, represented by a set of surfaces
    void addPolytope(SimpleIdentity selectId,const std::vector<Point3dVector > &surfaces,float minVis,float maxVis,bool enable);

    /// Add a linear in 3-space for selection.
    void addSelectableLinear(SimpleIdentity selectId,const Point3fVector &pts,float minVis,float maxVis,bool enable);

    /// Add a billboard for selection.  Pass in the middle of the base and size
    void addSelectableBillboard(SimpleIdentity selectId,Point3f center,Point3f norm,Point2f size,float minVis,float maxVis,bool enable);
    
    /// Remove the given selectable from consideration
    void removeSelectable(SimpleIdentity selectId);
    
    /// Remove a set of selectables from consideration
    void removeSelectables(const SimpleIDSet &selectIDs);
    
    /// Enable/disable selectable
    void enableSelectable(SimpleIdentity selectID,bool enable);
    
    /// Enable/disable a set of selectables
    void enableSelectables(const SimpleIDSet &selectIDs,bool enable);
    
    /// Pass in the view point where the user touched.  This returns the closest hit within the given distance
    SimpleIdentity pickObject(Point2f touchPt,float maxDist,WhirlyKit::View *theView);
    
    /// Find all the objects within a given distance and return them, sorted by distance
    void pickObjects(Point2f touchPt,float maxDist,View *theView,std::vector<SelectedObject> &selObjs);
    
    // Everything we need to project a world coordinate to one or more screen locations
    class PlacementInfo
    {
    public:
        PlacementInfo(View *view,SceneRendererES *renderer);
        
        WhirlyGlobe::GlobeView *globeView;
        Maply::MapView *mapView;
        double heightAboveSurface;
        Eigen::Matrix4d viewMat,modelMat,viewAndModelMat,viewAndModelInvMat,viewModelNormalMat,projMat,modelInvMat;
        std::vector<Eigen::Matrix4d> offsetMatrices;
        Point2f frameSize;
        Point2f frameSizeScale;
        Mbr frameMbr;
    };

protected:
    // Projects a world coordinate to one or more points on the screen (wrapping)
    void projectWorldPointToScreen(const Point3d &worldLoc,const PlacementInfo &pInfo,Point2dVector &screenPts,float scale);
    // Convert rect selectables into more generic screen space objects
    void getScreenSpaceObjects(const PlacementInfo &pInfo,std::vector<ScreenSpaceObjectLocation> &screenObjs);
    // Internal object picking method
    void pickObjects(Point2f touchPt,float maxDist,View *theView,bool multi,std::vector<SelectedObject> &selObjs);


    pthread_mutex_t mutex;
    Scene *scene;
    float scale;
    /// The selectable objects themselves
    WhirlyKit::RectSelectable3DSet rect3Dselectables;
    WhirlyKit::RectSelectable2DSet rect2Dselectables;
    WhirlyKit::PolytopeSelectableSet polytopeSelectables;
    WhirlyKit::LinearSelectableSet linearSelectables;
    WhirlyKit::BillboardSelectableSet billboardSelectables;
};
 
}
