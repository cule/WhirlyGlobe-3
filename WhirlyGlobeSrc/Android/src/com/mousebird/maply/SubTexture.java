/*
 *  SubTexture.java
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

import java.util.List;

public class SubTexture {

    long id = Identifiable.genID();

    public SubTexture(){
        initialise();
    }

    public void finalise(){
        dispose();
    }

    public native void setFromTex(TexCoord texCoord, TexCoord texDest);

    public native TexCoord procressTexCoord(TexCoord texCoord);

    public native void processTexCoords(List<TexCoord> data);

    public native void setTexID(int texID);

    public native int getTexID();

    static
    {
        nativeInit();
    }
    private static native void nativeInit();
    native void initialise();
    native void dispose();
}
