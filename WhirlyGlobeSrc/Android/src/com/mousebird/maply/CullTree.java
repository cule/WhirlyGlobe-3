/*
 *  CullTree.java
 *  WhirlyGlobeLib
 *
 *  Created by jmnavarro
 *  Copyright 2011-2015 mousebird consulting
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
package com.mousebird.maply;

public class CullTree {

    public CullTree(CoordSystemDisplayAdapter coordSystem, Mbr localMbr, int depth, int maxDrawPerNode){

        initialise(coordSystem,localMbr.ll, localMbr.ur,depth,maxDrawPerNode);
    }

    public void finalise(){
        dispose();
    }

    public native Cullable getTopCullable();

    public native int getCount();

    public native void dumpStats();

    native void dispose();

    native void initialise(CoordSystemDisplayAdapter coordSystem, Point2d ll,Point2d ur, int depth, int maxDrawPerNode);
    static
    {
        nativeInit();
    }
    private static native void nativeInit();
    private long nativeHandle;
}
