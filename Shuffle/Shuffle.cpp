/*
 OFX Shuffle plugin.

 Copyright (C) 2014 INRIA

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France


 The skeleton for this source file is from:
 OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.

 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England

 */

#include "Shuffle.h"

#include <cmath>
#ifdef _WINDOWS
#include <windows.h>
#endif
#include <set>

#include "ofxsProcessing.H"
#include "ofxsMacros.h"

#define kPluginName "ShuffleOFX"
#define kPluginGrouping "Channel"
#define kPluginDescription "Rearrange channels from one or two inputs and/or convert to different bit depth or components. No colorspace conversion is done (mapping is linear, even for 8-bit and 16-bit types)."
#define kPluginIdentifier "net.sf.openfx.ShufflePlugin"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths true // can convert depth
#define kRenderThreadSafety eRenderFullySafe

#define kEnableMultiPlanar false

#define kParamOutputComponents "outputComponents"
#define kParamOutputComponentsLabel "Output Components"
#define kParamOutputComponentsHint "Components in the output"
#define kParamOutputComponentsOptionRGBA "RGBA"
#define kParamOutputComponentsOptionRGB "RGB"
#define kParamOutputComponentsOptionAlpha "Alpha"

#define kParamOutputBitDepth "outputBitDepth"
#define kParamOutputBitDepthLabel "Output Bit Depth"
#define kParamOutputBitDepthHint "Bit depth of the output.\nWARNING: the conversion is linear, even for 8-bit or 16-bit depth. Use with care."
#define kParamOutputBitDepthOptionByte "Byte (8 bits)"
#define kParamOutputBitDepthOptionShort "Short (16 bits)"
#define kParamOutputBitDepthOptionFloat "Float (32 bits)"

#define kParamOutputR "outputR"
#define kParamOutputRLabel "Output R"
#define kParamOutputRHint "Input channel for the output red channel"

#define kParamOutputG "outputG"
#define kParamOutputGLabel "Output G"
#define kParamOutputGHint "Input channel for the output green channel"

#define kParamOutputB "outputB"
#define kParamOutputBLabel "Output B"
#define kParamOutputBHint "Input channel for the output blue channel"

#define kParamOutputA "outputA"
#define kParamOutputALabel "Output A"
#define kParamOutputAHint "Input channel for the output alpha channel"

#define kParamOutputOptionAR "A.r"
#define kParamOutputOptionARHint "R channel from input A"
#define kParamOutputOptionAG "A.g"
#define kParamOutputOptionAGHint "G channel from input A"
#define kParamOutputOptionAB "A.b"
#define kParamOutputOptionABHint "B channel from input A"
#define kParamOutputOptionAA "A.a"
#define kParamOutputOptionAAHint "A channel from input A"
#define kParamOutputOption0 "0"
#define kParamOutputOption0Hint "0 constant channel"
#define kParamOutputOption1 "1"
#define kParamOutputOption1Hint "1 constant channel"
#define kParamOutputOptionBR "B.r"
#define kParamOutputOptionBRHint "R channel from input B"
#define kParamOutputOptionBG "B.g"
#define kParamOutputOptionBGHint "G channel from input B"
#define kParamOutputOptionBB "B.b"
#define kParamOutputOptionBBHint "B channel from input B"
#define kParamOutputOptionBA "B.a"
#define kParamOutputOptionBAHint "A channel from input B"

#define kShuffleColorPlaneName "Color"
#define kShuffleMotionBackwardPlaneName "Backward"
#define kShuffleMotionForwardPlaneName "Forward"
#define kShuffleDisparityLeftPlaneName "DisparityLeft"
#define kShuffleDisparityRightPlaneName "DisparityRight"

#define kParamClipInfo "clipInfo"
#define kParamClipInfoLabel "Clip Info..."
#define kParamClipInfoHint "Display information about the inputs"

// TODO: sRGB/Rec.709 conversions for byte/short types

enum InputChannelEnum {
    eInputChannelAR = 0,
    eInputChannelAG,
    eInputChannelAB,
    eInputChannelAA,
    eInputChannel0,
    eInputChannel1,
    eInputChannelBR,
    eInputChannelBG,
    eInputChannelBB,
    eInputChannelBA,
};

#define kClipA "A"
#define kClipB "B"

static bool gSupportsBytes  = false;
static bool gSupportsShorts = false;
static bool gSupportsFloats = false;
static bool gSupportsRGBA   = false;
static bool gSupportsRGB    = false;
static bool gSupportsAlpha  = false;
static bool gSupportsDynamicChoices = false;
static bool gIsMultiPlanar = false;

static OFX::PixelComponentEnum gOutputComponentsMap[4];
static OFX::BitDepthEnum gOutputBitDepthMap[4];

using namespace OFX;


static int nComps(PixelComponentEnum e)
{
    switch (e) {
        case OFX::ePixelComponentRGBA:
            return 4;
        case OFX::ePixelComponentRGB:
            return 3;
        case OFX::ePixelComponentAlpha:
            return 1;
        default:
            return 0;
    }
}

class ShufflerBase : public OFX::ImageProcessor
{
protected:
    const OFX::Image *_srcImgA;
    const OFX::Image *_srcImgB;
    PixelComponentEnum _outputComponents;
    BitDepthEnum _outputBitDepth;
    int _nComponentsDst;
    InputChannelEnum _channelMap[4];

    public:
    ShufflerBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImgA(0)
    , _srcImgB(0)
    , _outputComponents(ePixelComponentNone)
    , _outputBitDepth(eBitDepthNone)
    , _nComponentsDst(0)
    {
        for (int c = 0; c < 4; ++c) {
            _channelMap[c] = eInputChannel0;
        }
    }

    void setSrcImg(const OFX::Image *A, const OFX::Image *B) {_srcImgA = A; _srcImgB = B;}

    void setValues(PixelComponentEnum outputComponents,
                   BitDepthEnum outputBitDepth,
                   InputChannelEnum *channelMap)
    {
        _outputComponents = outputComponents,
        _outputBitDepth = outputBitDepth;
        _nComponentsDst = nComps(outputComponents);
        for (int c = 0; c < _nComponentsDst; ++c) {
            _channelMap[c] = channelMap[c];
        }
    }
};

//////////////////////////////
// PIXEL CONVERSION ROUTINES

/// maps 0-(numvals-1) to 0.-1.
template<int numvals>
static float intToFloat(int value)
{
    return value / (float)(numvals-1);
}

/// maps °.-1. to 0-(numvals-1)
template<int numvals>
static int floatToInt(float value)
{
    if (value <= 0) {
        return 0;
    } else if (value >= 1.) {
        return numvals - 1;
    }
    return value * (numvals-1) + 0.5f;
}

template <typename SRCPIX,typename DSTPIX>
static DSTPIX convertPixelDepth(SRCPIX pix);

///explicit template instantiations

template <> float convertPixelDepth(unsigned char pix)
{
    return intToFloat<65536>(pix);
}

template <> unsigned short convertPixelDepth(unsigned char pix)
{
    // 0x01 -> 0x0101, 0x02 -> 0x0202, ..., 0xff -> 0xffff
    return (unsigned short)((pix << 8) + pix);
}

template <> unsigned char convertPixelDepth(unsigned char pix)
{
    return pix;
}

template <> unsigned char convertPixelDepth(unsigned short pix)
{
    // the following is from ImageMagick's quantum.h
    return (unsigned char)(((pix+128UL)-((pix+128UL) >> 8)) >> 8);
}

template <> float convertPixelDepth(unsigned short pix)
{
    return intToFloat<65536>(pix);
}

template <> unsigned short convertPixelDepth(unsigned short pix)
{
    return pix;
}

template <> unsigned char convertPixelDepth(float pix)
{
    return (unsigned char)floatToInt<256>(pix);
}

template <> unsigned short convertPixelDepth(float pix)
{
    return (unsigned short)floatToInt<65536>(pix);
}

template <> float convertPixelDepth(float pix)
{
    return pix;
}


template <class PIXSRC, class PIXDST, int nComponentsDst>
class Shuffler : public ShufflerBase
{
public:
    Shuffler(OFX::ImageEffect &instance)
    : ShufflerBase(instance)
    {
    }

private:
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        const OFX::Image* channelMapImg[nComponentsDst];
        int channelMapComp[nComponentsDst]; // channel component, or value if no image
        int srcMapComp[4]; // R,G,B,A components for src
        PixelComponentEnum srcComponents = ePixelComponentNone;
        if (_srcImgA) {
            srcComponents = _srcImgA->getPixelComponents();
        } else if (_srcImgB) {
            srcComponents = _srcImgB->getPixelComponents();
        }
        switch (srcComponents) {
            case OFX::ePixelComponentRGBA:
                srcMapComp[0] = 0;
                srcMapComp[1] = 1;
                srcMapComp[2] = 2;
                srcMapComp[3] = 3;
                break;
            case OFX::ePixelComponentRGB:
                srcMapComp[0] = 0;
                srcMapComp[1] = 1;
                srcMapComp[2] = 2;
                srcMapComp[3] = -1;
                break;
            case OFX::ePixelComponentAlpha:
                srcMapComp[0] = -1;
                srcMapComp[1] = -1;
                srcMapComp[2] = -1;
                srcMapComp[3] = 0;
                break;
            default:
                srcMapComp[0] = -1;
                srcMapComp[1] = -1;
                srcMapComp[2] = -1;
                srcMapComp[3] = -1;
                break;
        }
        for (int c = 0; c < nComponentsDst; ++c) {
            channelMapImg[c] = NULL;
            channelMapComp[c] = 0;
            switch (_channelMap[c]) {
                case eInputChannelAR:
                    if (_srcImgA && srcMapComp[0] >= 0) {
                        channelMapImg[c] = _srcImgA;
                        channelMapComp[c] = srcMapComp[0]; // srcImg may not have R!!!
                    }
                    break;
                case eInputChannelAG:
                    if (_srcImgA && srcMapComp[1] >= 0) {
                        channelMapImg[c] = _srcImgA;
                        channelMapComp[c] = srcMapComp[1];
                    }
                    break;
                case eInputChannelAB:
                    if (_srcImgA && srcMapComp[2] >= 0) {
                        channelMapImg[c] = _srcImgA;
                        channelMapComp[c] = srcMapComp[2];
                    }
                    break;
                case eInputChannelAA:
                    if (_srcImgA && srcMapComp[3] >= 0) {
                        channelMapImg[c] = _srcImgA;
                        channelMapComp[c] = srcMapComp[3];
                    }
                    break;
                case eInputChannel0:
                    channelMapComp[c] = 0;
                    break;
                case eInputChannel1:
                    channelMapComp[c] = 1;
                    break;
                case eInputChannelBR:
                    if (_srcImgB && srcMapComp[0] >= 0) {
                        channelMapImg[c] = _srcImgB;
                        channelMapComp[c] = srcMapComp[0];
                    }
                    break;
                case eInputChannelBG:
                    if (_srcImgB && srcMapComp[1] >= 0) {
                        channelMapImg[c] = _srcImgB;
                        channelMapComp[c] = srcMapComp[1];
                    }
                    break;
                case eInputChannelBB:
                    if (_srcImgB && srcMapComp[2] >= 0) {
                        channelMapImg[c] = _srcImgB;
                        channelMapComp[c] = srcMapComp[2];
                    }
                    break;
                case eInputChannelBA:
                    if (_srcImgB && srcMapComp[3] >= 0) {
                        channelMapImg[c] = _srcImgB;
                        channelMapComp[c] = srcMapComp[3];
                    }
                    break;
            }
        }
        // now compute the transformed image, component by component
        for (int c = 0; c < nComponentsDst; ++c) {
            const OFX::Image* srcImg = channelMapImg[c];
            int srcComp = channelMapComp[c];

            for (int y = procWindow.y1; y < procWindow.y2; y++) {
                if (_effect.abort()) {
                    break;
                }

                PIXDST *dstPix = (PIXDST *) _dstImg->getPixelAddress(procWindow.x1, y);

                for (int x = procWindow.x1; x < procWindow.x2; x++) {
                    PIXSRC *srcPix = (PIXSRC *)  (srcImg ? srcImg->getPixelAddress(x, y) : 0);
                    // if there is a srcImg but we are outside of its RoD, it should be considered black and transparent
                    dstPix[c] = srcImg ? convertPixelDepth<PIXSRC,PIXDST>(srcPix ? srcPix[srcComp] : 0) : convertPixelDepth<float,PIXDST>(srcComp);
                    dstPix += nComponentsDst;
                }
            }
        }
    }
};

struct InputPlaneChannel {
    OFX::Image* img;
    int channelIndex;
    bool fillZero;
    
    InputPlaneChannel() : img(0), channelIndex(-1), fillZero(true) {}
};

class MultiPlaneShufflerBase : public OFX::ImageProcessor
{
protected:
    
    PixelComponentEnum _outputComponents;
    BitDepthEnum _outputBitDepth;
    int _nComponentsDst;
    std::vector<InputPlaneChannel> _inputPlanes;

public:
    MultiPlaneShufflerBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _outputComponents(ePixelComponentNone)
    , _outputBitDepth(eBitDepthNone)
    , _nComponentsDst(0)
    , _inputPlanes(_nComponentsDst)
    {
    }

    void setValues(PixelComponentEnum outputComponents,
                   BitDepthEnum outputBitDepth,
                   const std::vector<InputPlaneChannel>& planes)
    {
        _outputComponents = outputComponents,
        _outputBitDepth = outputBitDepth;
        _nComponentsDst = nComps(outputComponents);
        _inputPlanes = planes;

    }
};


template <class PIXSRC, class PIXDST, int nComponentsDst>
class MultiPlaneShuffler : public MultiPlaneShufflerBase
{
public:
    MultiPlaneShuffler(OFX::ImageEffect &instance)
    : MultiPlaneShufflerBase(instance)
    {
    }
    
private:
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        assert(_inputPlanes.size() == nComponentsDst);
        // now compute the transformed image, component by component
        for (int c = 0; c < nComponentsDst; ++c) {
            
            const OFX::Image* srcImg = _inputPlanes[c].img;
            int srcComp = _inputPlanes[c].channelIndex;
            if (!srcImg) {
                srcComp = _inputPlanes[c].fillZero ? 0. : 1.;
            }
            
            for (int y = procWindow.y1; y < procWindow.y2; y++) {
                if (_effect.abort()) {
                    break;
                }
                
                PIXDST *dstPix = (PIXDST *) _dstImg->getPixelAddress(procWindow.x1, y);
                
                for (int x = procWindow.x1; x < procWindow.x2; x++) {
                    PIXSRC *srcPix = (PIXSRC *)  (srcImg ? srcImg->getPixelAddress(x, y) : 0);
                    // if there is a srcImg but we are outside of its RoD, it should be considered black and transparent
                    dstPix[c] = srcImg ? convertPixelDepth<PIXSRC,PIXDST>(srcPix ? srcPix[srcComp] : 0) : convertPixelDepth<float,PIXDST>(srcComp);
                    dstPix += nComponentsDst;
                }
            }
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class ShufflePlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    ShufflePlugin(OfxImageEffectHandle handle, OFX::ContextEnum context)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClipA(0)
    , _srcClipB(0)
    , _outputComponents(0)
    , _outputBitDepth(0)
    , _r(0)
    , _g(0)
    , _b(0)
    , _a(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA || _dstClip->getPixelComponents() == ePixelComponentAlpha));
        _srcClipA = fetchClip(context == eContextGeneral ? kClipA : kOfxImageEffectSimpleSourceClipName);
        assert(_srcClipA && (_srcClipA->getPixelComponents() == ePixelComponentRGB || _srcClipA->getPixelComponents() == ePixelComponentRGBA || _srcClipA->getPixelComponents() == ePixelComponentAlpha));
        if (context == eContextGeneral) {
            _srcClipB = fetchClip(kClipB);
            assert(_srcClipB && (_srcClipB->getPixelComponents() == ePixelComponentRGB || _srcClipB->getPixelComponents() == ePixelComponentRGBA || _srcClipB->getPixelComponents() == ePixelComponentAlpha));
        }
        _outputComponents = fetchChoiceParam(kParamOutputComponents);
        if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
            _outputBitDepth = fetchChoiceParam(kParamOutputBitDepth);
        }
        _r = fetchChoiceParam(kParamOutputR);
        _g = fetchChoiceParam(kParamOutputG);
        _b = fetchChoiceParam(kParamOutputB);
        _a = fetchChoiceParam(kParamOutputA);
        enableComponents();
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    /* override is identity */
    virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime) OVERRIDE FINAL;

    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
    
    virtual void getClipComponents(const OFX::ClipComponentsArguments& args, OFX::ClipComponentsSetter& clipComponents) OVERRIDE FINAL;

    /** @brief get the clip preferences */
    virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    /* override changedParam */
    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    /** @brief called when a clip has just been changed in some way (a rewire maybe) */
    virtual void changedClip(const InstanceChangedArgs &args, const std::string &clipName) OVERRIDE FINAL;

private:
    
    bool isIdentityInternal(OFX::Clip*& identityClip);
    
    bool getPlaneNeededForParam(const std::list<std::string>& aComponents,
                                const std::list<std::string>& bComponents,
                                OFX::ChoiceParam* param,
                                OFX::Clip** clip,
                                std::string* ofxPlane,
                                std::string* ofxComponents,
                                int* channelIndexInPlane) const;
    
    void buildChannelMenus();
    
    void enableComponents(void);

    /* internal render function */
    template <class DSTPIX, int nComponentsDst>
    void renderInternalForDstBitDepth(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth);

    template <int nComponentsDst>
    void renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth, OFX::BitDepthEnum dstBitDepth);

    /* set up and run a processor */
    void setupAndProcess(ShufflerBase &, const OFX::RenderArguments &args);
    void setupAndProcessMultiPlane(MultiPlaneShufflerBase &, const OFX::RenderArguments &args);

    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *_dstClip;
    OFX::Clip *_srcClipA;
    OFX::Clip *_srcClipB;

    OFX::ChoiceParam *_outputComponents;
    OFX::ChoiceParam *_outputBitDepth;
    OFX::ChoiceParam *_r;
    OFX::ChoiceParam *_g;
    OFX::ChoiceParam *_b;
    OFX::ChoiceParam *_a;
};




static void extractChannelsFromComponentString(const std::string& comp,
                                               std::string* layer,
                                               std::string* pairedLayer, //< if disparity or motion vectors
                                               std::vector<std::string>* channels)
{
    if (comp == kOfxImageComponentAlpha) {
        *layer = kShuffleColorPlaneName;
        channels->push_back("a");
    } else if (comp == kOfxImageComponentRGB) {
        *layer = kShuffleColorPlaneName;
        channels->push_back("r");
        channels->push_back("g");
        channels->push_back("b");
    } else if (comp == kOfxImageComponentRGBA) {
        *layer = kShuffleColorPlaneName;
        channels->push_back("r");
        channels->push_back("g");
        channels->push_back("b");
        channels->push_back("a");
    } else if (comp == kFnOfxImageComponentMotionVectors) {
        *layer = kShuffleMotionBackwardPlaneName;
        *pairedLayer = kShuffleMotionForwardPlaneName;
        channels->push_back("u");
        channels->push_back("v");
    } else if (comp == kFnOfxImageComponentStereoDisparity) {
        *layer = kShuffleDisparityLeftPlaneName;
        *pairedLayer = kShuffleDisparityRightPlaneName;
        channels->push_back("x");
        channels->push_back("y");
    } else {
        try {
            OFX::ImageBase::ofxCustomCompToNatronComp(comp, layer, channels);
        } catch (const std::exception & /*e*/) {
            //unrecognized components, ignore it.
        }
    }
}

static void appendComponents(const std::string& clipName,
                             const std::list<std::string>& components,
                             OFX::ChoiceParam** params)
{
    for (std::list<std::string>::const_iterator it = components.begin(); it!=components.end(); ++it) {
        std::string layer,secondLayer;
        std::vector<std::string> channels;
        extractChannelsFromComponentString(*it, &layer, &secondLayer, &channels);
        if (layer.empty() || channels.empty()) {
            continue;
        }
        for (std::size_t i = 0; i < channels.size(); ++i) {
            std::string opt = clipName + ".";
            if (!layer.empty()) {
                opt.append(layer);
                opt.push_back('.');
            }
            opt.append(channels[i]);
            
            for (int j = 0; j < 4; ++j) {
                params[j]->appendOption(opt);
            }
        }
        
        
        
        if (!secondLayer.empty()) {
            for (std::size_t i = 0; i < channels.size(); ++i) {
                std::string opt = clipName + ".";
                if (!secondLayer.empty()) {
                    opt.append(layer);
                    opt.push_back('.');
                }
                opt.append(channels[i]);
                for (int j = 0; j < 4; ++j) {
                    params[j]->appendOption(opt);
                }
            }
        }
    }
}

void
ShufflePlugin::buildChannelMenus()
{
    assert(gSupportsDynamicChoices);
    
    std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
    std::list<std::string> componentsB = _srcClipB->getComponentsPresent();
    _r->resetOptions();
    _g->resetOptions();
    _b->resetOptions();
    _a->resetOptions();
    
    OFX::ChoiceParam* params[4] = {_r, _g, _b, _a};
    appendComponents(kClipA, componentsA, params);
    for (int i = 0; i < 4; ++i) {
        params[i]->appendOption(kParamOutputOption0,kParamOutputOption0Hint);
        params[i]->appendOption(kParamOutputOption1,kParamOutputOption1Hint);
    }
    appendComponents(kClipB, componentsB, params);
}

bool
ShufflePlugin::getPlaneNeededForParam(const std::list<std::string>& aComponents,
                                      const std::list<std::string>& bComponents,
                                      OFX::ChoiceParam* param,
                                      OFX::Clip** clip,
                                      std::string* ofxPlane,
                                      std::string* ofxComponents,
                                      int* channelIndexInPlane) const
{
    assert(clip);
    *clip = 0;
    
    int channelIndex;
    param->getValue(channelIndex);
    std::string channelEncoded;
    param->getOption(channelIndex, channelEncoded);
    if (channelEncoded.empty()) {
        return false;
    }
    
    if (channelEncoded == kParamOutputOption0) {
        *ofxComponents =  kParamOutputOption0;
        return true;
    }
    
    if (channelEncoded == kParamOutputOption1) {
        *ofxComponents = kParamOutputOption1;
        return true;
    }
    
    std::string clipName = kClipA;
    
    // Must be at least something like "A."
    if (channelEncoded.size() < clipName.size() + 1) {
        return false;
    }
    
    if (channelEncoded.substr(0,clipName.size()) == clipName) {
        *clip = _srcClipA;
    }
    
    if (!*clip) {
        clipName = kClipB;
        if (channelEncoded.substr(0,clipName.size()) == clipName) {
            *clip = _srcClipB;
        }
    }
    
    if (!*clip) {
        return false;
    }
    
    std::size_t lastDotPos = channelEncoded.find_last_of('.');
    if (lastDotPos == std::string::npos || lastDotPos == channelEncoded.size() - 1) {
        *clip = 0;
        return false;
    }
    
    std::string chanName = channelEncoded.substr(lastDotPos + 1,std::string::npos);
    std::string layerName;
    for (std::size_t i = clipName.size() + 1; i < lastDotPos; ++i) {
        layerName.push_back(channelEncoded[i]);
    }
    
    if (layerName == kShuffleColorPlaneName) {
        std::string comp = (*clip)->getPixelComponentsProperty();
        if (chanName == "r") {
            *channelIndexInPlane = 0;
        } else if (chanName == "g") {
            *channelIndexInPlane = 1;
        } else if (chanName == "b") {
            *channelIndexInPlane = 2;
        } else if (chanName == "a") {
            assert(comp == kOfxImageComponentAlpha || comp == kOfxImageComponentRGBA);
            *channelIndexInPlane = comp == kOfxImageComponentAlpha ? 0 : 3;
        } else {
            assert(false);
        }
        *ofxComponents = comp;
        *ofxPlane = kFnOfxImagePlaneColour;
        return true;
    } else if (layerName == kShuffleDisparityLeftPlaneName) {
        if (chanName == "x") {
            *channelIndexInPlane = 0;
        } else if (chanName == "y") {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kFnOfxImageComponentStereoDisparity;
        *ofxPlane = kFnOfxImagePlaneStereoDisparityLeft;
        return true;
    } else if (layerName == kShuffleDisparityRightPlaneName) {
        if (chanName == "x") {
            *channelIndexInPlane = 0;
        } else if (chanName == "y") {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kFnOfxImageComponentStereoDisparity;
        *ofxPlane =  kFnOfxImagePlaneStereoDisparityRight;
        return true;
    } else if (layerName == kShuffleMotionBackwardPlaneName) {
        if (chanName == "u") {
            *channelIndexInPlane = 0;
        } else if (chanName == "v") {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kFnOfxImageComponentMotionVectors;
        *ofxPlane = kFnOfxImagePlaneBackwardMotionVector;
    } else if (layerName == kShuffleMotionForwardPlaneName) {
        if (chanName == "u") {
            *channelIndexInPlane = 0;
        } else if (chanName == "v") {
            *channelIndexInPlane = 1;
        } else {
            assert(false);
        }
        *ofxComponents = kFnOfxImageComponentMotionVectors;
        *ofxPlane = kFnOfxImagePlaneForwardMotionVector;
    } else {
        //Find in aComponents or bComponents a layer matching the name of the layer
        for (std::list<std::string>::const_iterator it = aComponents.begin(); it!=aComponents.end(); ++it) {
            if (it->find(layerName) != std::string::npos) {
                //We found a matching layer
                std::string realLayerName;
                std::vector<std::string> channels;
                try {
                    OFX::ImageBase::ofxCustomCompToNatronComp(*it, &realLayerName, &channels);
                } catch (...) {
                    // ignore it
                    continue;
                }
                int foundChannel = -1;
                for (std::size_t i = 0; i < channels.size(); ++i) {
                    if (channels[i] == chanName) {
                        foundChannel = i;
                        break;
                    }
                }
                assert(foundChannel != -1);
                *ofxPlane = *it;
                *ofxComponents = *it;
                return true;
            }
        }
        
        for (std::list<std::string>::const_iterator it = bComponents.begin(); it!=bComponents.end(); ++it) {
            if (it->find(layerName) != std::string::npos) {
                //We found a matching layer
                std::string realLayerName;
                std::vector<std::string> channels;
                try {
                    OFX::ImageBase::ofxCustomCompToNatronComp(*it, &realLayerName, &channels);
                } catch (...) {
                    // ignore it
                    continue;
                }
                int foundChannel = -1;
                for (std::size_t i = 0; i < channels.size(); ++i) {
                    if (channels[i] == chanName) {
                        foundChannel = i;
                        break;
                    }
                }
                assert(foundChannel != -1);
                *ofxPlane = *it;
                *ofxComponents = *it;
                return true;
            }
        }
    }
    return false;
}


void
ShufflePlugin::getClipComponents(const OFX::ClipComponentsArguments& /*args*/, OFX::ClipComponentsSetter& clipComponents)
{
    
    std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
    std::list<std::string> componentsB = _srcClipB->getComponentsPresent();
    
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
    clipComponents.addClipComponents(*_dstClip, outputComponents);
    
    OFX::ChoiceParam* params[4] = {_r, _g, _b, _a};
    
    std::map<OFX::Clip*,std::set<std::string> > clipMap;
    
    for (int i = 0; i < 4; ++i) {
        std::string ofxComp,ofxPlane;
        int channelIndex;
        OFX::Clip* clip = 0;
        bool ok = getPlaneNeededForParam(componentsA, componentsB, params[i], &clip, &ofxPlane, &ofxComp, &channelIndex);
        assert(ok);
        if (ofxComp == kParamOutputOption0 || ofxComp == kParamOutputOption1) {
            continue;
        }
        assert(clip);
        
        std::map<OFX::Clip*,std::set<std::string> >::iterator foundClip = clipMap.find(clip);
        if (foundClip == clipMap.end()) {
            std::set<std::string> s;
            s.insert(ofxComp);
            clipMap.insert(std::make_pair(clip, s));
            clipComponents.addClipComponents(*clip, ofxComp);
        } else {
            std::pair<std::set<std::string>::iterator,bool> ret = foundClip->second.insert(ofxComp);
            if (ret.second) {
                clipComponents.addClipComponents(*clip, ofxComp);
            }
        }
    }
}

struct IdentityChoiceData
{
    OFX::Clip* clip;
    std::string components;
    int index;
};

bool
ShufflePlugin::isIdentityInternal(OFX::Clip*& identityClip)
{
    if (!gSupportsDynamicChoices || !gIsMultiPlanar) {
        int r_i;
        _r->getValue(r_i);
        InputChannelEnum r = InputChannelEnum(r_i);
        int g_i;
        _g->getValue(g_i);
        InputChannelEnum g = InputChannelEnum(g_i);
        int b_i;
        _b->getValue(b_i);
        InputChannelEnum b = InputChannelEnum(b_i);
        int a_i;
        _a->getValue(a_i);
        InputChannelEnum a = InputChannelEnum(a_i);
        
        if (r == eInputChannelAR && g == eInputChannelAG && b == eInputChannelAB && a == eInputChannelAA && _srcClipA) {
            identityClip = _srcClipA;
            
            return true;
        }
        if (r == eInputChannelBR && g == eInputChannelBG && b == eInputChannelBB && a == eInputChannelBA && _srcClipB) {
            identityClip = _srcClipB;
            
            return true;
        }
        return false;
    } else {
        std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
        std::list<std::string> componentsB = _srcClipB->getComponentsPresent();
        
        OFX::ChoiceParam* params[4] = {_r, _g, _b, _a};
        IdentityChoiceData data[4];
        
        int expectedIndex = -1;
        for (int i = 0; i < 4; ++i) {
            std::string plane;
            bool ok = getPlaneNeededForParam(componentsA, componentsB, params[i], &data[i].clip, &plane, &data[i].components, &data[i].index);
            assert(ok);
            
            if (i > 0) {
                if (data[i].index != expectedIndex || data[i].components != data[0].components ||
                    data[i].clip != data[0].clip) {
                    return false;
                }
            }
            expectedIndex = data[i].index + 1;
        }
        identityClip = data[0].clip;
        return true;
    }
}

bool
ShufflePlugin::isIdentity(const OFX::IsIdentityArguments &/*args*/, OFX::Clip * &identityClip, double &/*identityTime*/)
{
    return isIdentityInternal(identityClip);
}

bool
ShufflePlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    OFX::Clip* identityClip = 0;
    if (isIdentityInternal(identityClip)) {
        rod = identityClip->getRegionOfDefinition(args.time);
        return true;
    }
    if (_srcClipA && _srcClipA->isConnected() && _srcClipB && _srcClipB->isConnected()) {
        OfxRectD rodA = _srcClipA->getRegionOfDefinition(args.time);
        OfxRectD rodB = _srcClipB->getRegionOfDefinition(args.time);
        rod.x1 = std::min(rodA.x1, rodB.x1);
        rod.y1 = std::min(rodA.y1, rodB.y1);
        rod.x2 = std::max(rodA.x2, rodB.x2);
        rod.y2 = std::max(rodA.y2, rodB.y2);
        
        return true;
    }
    return false;
}


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */




/* set up and run a processor */
void
ShufflePlugin::setupAndProcess(ShufflerBase &processor, const OFX::RenderArguments &args)
{
    std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();
    if (dstBitDepth != _dstClip->getPixelDepth() ||
        dstComponents != _dstClip->getPixelComponents()) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if (dst->getRenderScale().x != args.renderScale.x ||
        dst->getRenderScale().y != args.renderScale.y ||
        (dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    
    InputChannelEnum r,g,b,a;
    // compute the components mapping tables
    InputChannelEnum channelMap[4];
    
    std::auto_ptr<const OFX::Image> srcA((_srcClipA && _srcClipA->isConnected()) ?
                                         _srcClipA->fetchImage(args.time) : 0);
    std::auto_ptr<const OFX::Image> srcB((_srcClipB && _srcClipB->isConnected()) ?
                                         _srcClipB->fetchImage(args.time) : 0);
    OFX::BitDepthEnum srcBitDepth = eBitDepthNone;
    OFX::PixelComponentEnum srcComponents = ePixelComponentNone;
    if (srcA.get()) {
        if (srcA->getRenderScale().x != args.renderScale.x ||
            srcA->getRenderScale().y != args.renderScale.y ||
            (srcA->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && srcA->getField() != args.fieldToRender)) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }
        srcBitDepth      = srcA->getPixelDepth();
        srcComponents = srcA->getPixelComponents();
        assert(_srcClipA->getPixelComponents() == srcComponents);
    }
    
    if (srcB.get()) {
        if (srcB->getRenderScale().x != args.renderScale.x ||
            srcB->getRenderScale().y != args.renderScale.y ||
            (srcB->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && srcB->getField() != args.fieldToRender)) {
            setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            OFX::throwSuiteStatusException(kOfxStatFailed);
        }
        OFX::BitDepthEnum    srcBBitDepth      = srcB->getPixelDepth();
        OFX::PixelComponentEnum srcBComponents = srcB->getPixelComponents();
        assert(_srcClipB->getPixelComponents() == srcBComponents);
        // both input must have the same bit depth and components
        if ((srcBitDepth != eBitDepthNone && srcBitDepth != srcBBitDepth) ||
            (srcComponents != ePixelComponentNone && srcComponents != srcBComponents)) {
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }
    
    int r_i;
    _r->getValue(r_i);
    r = InputChannelEnum(r_i);
    int g_i;
    _g->getValue(g_i);
    g = InputChannelEnum(g_i);
    int b_i;
    _b->getValue(b_i);
    b = InputChannelEnum(b_i);
    int a_i;
    _a->getValue(a_i);
    a = InputChannelEnum(a_i);
    
    
    switch (dstComponents) {
        case OFX::ePixelComponentRGBA:
            channelMap[0] = r;
            channelMap[1] = g;
            channelMap[2] = b;
            channelMap[3] = a;
            break;
        case OFX::ePixelComponentRGB:
            channelMap[0] = r;
            channelMap[1] = g;
            channelMap[2] = b;
            channelMap[3] = eInputChannel0;
            break;
        case OFX::ePixelComponentAlpha:
            channelMap[0] = a;
            channelMap[1] = eInputChannel0;
            channelMap[2] = eInputChannel0;
            channelMap[3] = eInputChannel0;
            break;
        default:
            channelMap[0] = eInputChannel0;
            channelMap[1] = eInputChannel0;
            channelMap[2] = eInputChannel0;
            channelMap[3] = eInputChannel0;
            break;
    }
    processor.setSrcImg(srcA.get(),srcB.get());
    
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
    BitDepthEnum outputBitDepth = srcBitDepth;
    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        int outputBitDepth_i;
        _outputBitDepth->getValue(outputBitDepth_i);
        outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
    }
    
    processor.setValues(outputComponents, outputBitDepth, channelMap);
    
    processor.setDstImg(dst.get());
    processor.setRenderWindow(args.renderWindow);

    processor.process();
}

class InputImagesHolder_RAII
{
    std::vector<OFX::Image*> images;
public:
    
    InputImagesHolder_RAII()
    : images()
    {
        
    }
    
    void appendImage(OFX::Image* img)
    {
        images.push_back(img);
    }
    
    ~InputImagesHolder_RAII()
    {
        for (std::size_t i = 0; i < images.size(); ++i) {
            delete images[i];
        }
    }
};

void
ShufflePlugin::setupAndProcessMultiPlane(MultiPlaneShufflerBase & processor, const OFX::RenderArguments &args)
{
    std::auto_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();
    if (dstBitDepth != _dstClip->getPixelDepth() ||
        dstComponents != _dstClip->getPixelComponents()) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if (dst->getRenderScale().x != args.renderScale.x ||
        dst->getRenderScale().y != args.renderScale.y ||
        (dst->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && dst->getField() != args.fieldToRender)) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    
    
    
    std::list<std::string> componentsA = _srcClipA->getComponentsPresent();
    std::list<std::string> componentsB = _srcClipB->getComponentsPresent();
    
    OFX::ChoiceParam* params[4] = {_r, _g, _b, _a};
    
    InputImagesHolder_RAII imagesHolder;
    OFX::BitDepthEnum srcBitDepth = eBitDepthNone;
    
    std::map<std::string,OFX::Image*> fetchedPlanes;
    
    std::vector<InputPlaneChannel> planes;
    for (int i = 0; i < 4; ++i) {
        
        InputPlaneChannel p;
        OFX::Clip* clip = 0;
        std::string plane,ofxComp;
        bool ok = getPlaneNeededForParam(componentsA, componentsB, params[i], &clip, &plane, &ofxComp, &p.channelIndex);
        assert(ok && clip);
        
        p.img = 0;
        if (ofxComp == kParamOutputOption0) {
            p.fillZero = true;
        } else if (ofxComp == kParamOutputOption1) {
            p.fillZero = false;
        } else {
            std::map<std::string,OFX::Image*>::iterator foundPlane = fetchedPlanes.find(plane);
            if (foundPlane == fetchedPlanes.end()) {
                p.img = clip->fetchImagePlane(args.time, args.renderView, plane.c_str());
                if (p.img) {
                    fetchedPlanes.insert(std::make_pair(plane, p.img));
                    imagesHolder.appendImage(p.img);
                }
            } else {
                p.img = foundPlane->second;
            }
        }
        
        if (p.img) {
            
            if (p.img->getRenderScale().x != args.renderScale.x ||
                p.img->getRenderScale().y != args.renderScale.y ||
                (p.img->getField() != OFX::eFieldNone /* for DaVinci Resolve */ && p.img->getField() != args.fieldToRender)) {
                setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
                OFX::throwSuiteStatusException(kOfxStatFailed);
            }
            if (srcBitDepth == eBitDepthNone) {
                srcBitDepth = p.img->getPixelDepth();
            } else {
                // both input must have the same bit depth and components
                if (srcBitDepth != eBitDepthNone && srcBitDepth != p.img->getPixelDepth()) {
                    OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
                }
            }
        }
    }
    
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
    BitDepthEnum outputBitDepth = srcBitDepth;
    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        int outputBitDepth_i;
        _outputBitDepth->getValue(outputBitDepth_i);
        outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
    }

    processor.setValues(outputComponents, outputBitDepth, planes);
    
    processor.setDstImg(dst.get());
    processor.setRenderWindow(args.renderWindow);
    
    processor.process();
}

template <class DSTPIX, int nComponentsDst>
void
ShufflePlugin::renderInternalForDstBitDepth(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth)
{
    if (!gIsMultiPlanar || !gSupportsDynamicChoices) {
        switch (srcBitDepth) {
            case OFX::eBitDepthUByte : {
                Shuffler<unsigned char, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthUShort : {
                Shuffler<unsigned short, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthFloat : {
                Shuffler<float, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else {
        switch (srcBitDepth) {
            case OFX::eBitDepthUByte : {
                MultiPlaneShuffler<unsigned char, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcessMultiPlane(fred, args);
            }
                break;
            case OFX::eBitDepthUShort : {
                MultiPlaneShuffler<unsigned short, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcessMultiPlane(fred, args);
            }
                break;
            case OFX::eBitDepthFloat : {
                MultiPlaneShuffler<float, DSTPIX, nComponentsDst> fred(*this);
                setupAndProcessMultiPlane(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }

    }
}

// the internal render function
template <int nComponentsDst>
void
ShufflePlugin::renderInternal(const OFX::RenderArguments &args, OFX::BitDepthEnum srcBitDepth, OFX::BitDepthEnum dstBitDepth)
{
    switch (dstBitDepth) {
        case OFX::eBitDepthUByte :
            renderInternalForDstBitDepth<unsigned char, nComponentsDst>(args, srcBitDepth);
            break;
        case OFX::eBitDepthUShort :
            renderInternalForDstBitDepth<unsigned short, nComponentsDst>(args, srcBitDepth);
            break;
        case OFX::eBitDepthFloat :
            renderInternalForDstBitDepth<float, nComponentsDst>(args, srcBitDepth);
            break;
        default :
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

// the overridden render function
void
ShufflePlugin::render(const OFX::RenderArguments &args)
{
    if (!_srcClipA || !_srcClipB || !_dstClip) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = _dstClip->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();
    assert(dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA || dstComponents == ePixelComponentAlpha);

    assert(kSupportsMultipleClipPARs   || _srcClipA->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || _srcClipA->getPixelDepth()       == _dstClip->getPixelDepth());
    assert(kSupportsMultipleClipPARs   || _srcClipB->getPixelAspectRatio() == _dstClip->getPixelAspectRatio());
    assert(kSupportsMultipleClipDepths || _srcClipB->getPixelDepth()       == _dstClip->getPixelDepth());
    // get the components of _dstClip
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
    if (dstComponents != outputComponents) {
        setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: OFX Host dit not take into account output components");
        OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
    }

    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        // get the bitDepth of _dstClip
        int outputBitDepth_i;
        _outputBitDepth->getValue(outputBitDepth_i);
        BitDepthEnum outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
        if (dstBitDepth != outputBitDepth) {
            setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: OFX Host dit not take into account output bit depth");
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }

    OFX::BitDepthEnum srcBitDepth = _srcClipA->getPixelDepth();

    if (_srcClipA->isConnected() && _srcClipB->isConnected()) {
        OFX::BitDepthEnum srcBBitDepth = _srcClipB->getPixelDepth();
        // both input must have the same bit depth
        if (srcBitDepth != srcBBitDepth) {
            setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: both inputs must have the same bit depth");
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }

    if (dstComponents == OFX::ePixelComponentRGBA) {
            renderInternal<4>(args, srcBitDepth, dstBitDepth);
    } else if (dstComponents == OFX::ePixelComponentRGB) {
            renderInternal<3>(args, srcBitDepth, dstBitDepth);
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
            renderInternal<1>(args, srcBitDepth, dstBitDepth);
    }
}


/* Override the clip preferences */
void
ShufflePlugin::getClipPreferences(OFX::ClipPreferencesSetter &clipPreferences)
{
    if (gIsMultiPlanar && gSupportsDynamicChoices) {
        buildChannelMenus();
    }
    
    // set the components of _dstClip
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    PixelComponentEnum outputComponents = gOutputComponentsMap[outputComponents_i];
    clipPreferences.setClipComponents(*_dstClip, outputComponents);

    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        // set the bitDepth of _dstClip
        int outputBitDepth_i;
        _outputBitDepth->getValue(outputBitDepth_i);
        BitDepthEnum outputBitDepth = gOutputBitDepthMap[outputBitDepth_i];
        clipPreferences.setClipBitDepth(*_dstClip, outputBitDepth);
    }
}

static std::string
imageFormatString(PixelComponentEnum components, BitDepthEnum bitDepth)
{
    std::string s;
    switch (components) {
        case OFX::ePixelComponentRGBA:
            s += "RGBA";
            break;
        case OFX::ePixelComponentRGB:
            s += "RGB";
            break;
        case OFX::ePixelComponentAlpha:
            s += "Alpha";
            break;
        case OFX::ePixelComponentCustom:
            s += "Custom";
            break;
        case OFX::ePixelComponentNone:
            s += "None";
            break;
        default:
            s += "[unknown components]";
            break;
    }
    switch (bitDepth) {
        case OFX::eBitDepthUByte:
            s += "8u";
            break;
        case OFX::eBitDepthUShort:
            s += "16u";
            break;
        case OFX::eBitDepthFloat:
            s += "32f";
            break;
        case OFX::eBitDepthCustom:
            s += "x";
            break;
        case OFX::eBitDepthNone:
            s += "0";
            break;
        default:
            s += "[unknown bit depth]";
            break;
    }
    return s;
}

void
ShufflePlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{
    if (paramName == kParamOutputComponents) {
        enableComponents();
    } else if (paramName == kParamClipInfo && args.reason == eChangeUserEdit) {
        std::string msg;
        msg += "Input A: ";
        if (!_srcClipA) {
            msg += "N/A";
        } else {
            msg += imageFormatString(_srcClipA->getPixelComponents(), _srcClipA->getPixelDepth());
        }
        msg += "\n";
        if (getContext() == eContextGeneral) {
            msg += "Input B: ";
            if (!_srcClipB) {
                msg += "N/A";
            } else {
                msg += imageFormatString(_srcClipB->getPixelComponents(), _srcClipB->getPixelDepth());
            }
            msg += "\n";
        }
        msg += "Output: ";
        if (!_dstClip) {
            msg += "N/A";
        } else {
            msg += imageFormatString(_dstClip->getPixelComponents(), _dstClip->getPixelDepth());
        }
        msg += "\n";
        sendMessage(OFX::Message::eMessageMessage, "", msg);
    }
}

void
ShufflePlugin::changedClip(const InstanceChangedArgs &/*args*/, const std::string &clipName)
{
    if (getContext() == eContextGeneral &&
        (clipName == kClipA || clipName == kClipB)) {
        // check that A and B are compatible if they're both connected
        if (_srcClipA && _srcClipA->isConnected() && _srcClipB && _srcClipB->isConnected()) {
            OFX::BitDepthEnum srcABitDepth = _srcClipA->getPixelDepth();
            OFX::BitDepthEnum srcBBitDepth = _srcClipB->getPixelDepth();
            // both input must have the same bit depth
            if (srcABitDepth != srcBBitDepth) {
                setPersistentMessage(OFX::Message::eMessageError, "", "Shuffle: both inputs must have the same bit depth");
                OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
            }
        }
    }
}

void
ShufflePlugin::enableComponents(void)
{
    int outputComponents_i;
    _outputComponents->getValue(outputComponents_i);
    switch (gOutputComponentsMap[outputComponents_i]) {
        case ePixelComponentRGBA:
            _r->setEnabled(true);
            _g->setEnabled(true);
            _b->setEnabled(true);
            _a->setEnabled(true);
            break;
        case ePixelComponentRGB:
            _r->setEnabled(true);
            _g->setEnabled(true);
            _b->setEnabled(true);
            _a->setEnabled(false);
            break;
        case ePixelComponentAlpha:
            _r->setEnabled(false);
            _g->setEnabled(false);
            _b->setEnabled(false);
            _a->setEnabled(true);
            break;
        default:
            assert(0);
            break;
    }
}


mDeclarePluginFactory(ShufflePluginFactory, {}, {});

void ShufflePluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        for (ImageEffectHostDescription::PixelDepthArray::const_iterator it = getImageEffectHostDescription()->_supportedPixelDepths.begin();
             it != getImageEffectHostDescription()->_supportedPixelDepths.end();
             ++it) {
            switch (*it) {
                case eBitDepthUByte:
                    gSupportsBytes  = true;
                    break;
                case eBitDepthUShort:
                    gSupportsShorts = true;
                    break;
                case eBitDepthFloat:
                    gSupportsFloats = true;
                    break;
                default:
                    // other bitdepths are not supported by this plugin
                    break;
            }
        }
    }
    {
        int i = 0;
        if (gSupportsFloats) {
            gOutputBitDepthMap[i] = eBitDepthFloat;
            ++i;
        }
        if (gSupportsShorts) {
            gOutputBitDepthMap[i] = eBitDepthUShort;
            ++i;
        }
        if (gSupportsBytes) {
            gOutputBitDepthMap[i] = eBitDepthUByte;
            ++i;
        }
        gOutputBitDepthMap[i] = eBitDepthNone;
    }
    for (ImageEffectHostDescription::PixelComponentArray::const_iterator it = getImageEffectHostDescription()->_supportedComponents.begin();
         it != getImageEffectHostDescription()->_supportedComponents.end();
         ++it) {
        switch (*it) {
            case ePixelComponentRGBA:
                gSupportsRGBA  = true;
                break;
            case ePixelComponentRGB:
                gSupportsRGB = true;
                break;
            case ePixelComponentAlpha:
                gSupportsAlpha = true;
                break;
            default:
                // other components are not supported by this plugin
                break;
        }
    }
    {
        int i = 0;
        if (gSupportsRGBA) {
            gOutputComponentsMap[i] = ePixelComponentRGBA;
            ++i;
        }
        if (gSupportsRGB) {
            gOutputComponentsMap[i] = ePixelComponentRGB;
            ++i;
        }
        if (gSupportsAlpha) {
            gOutputComponentsMap[i] = ePixelComponentAlpha;
            ++i;
        }
        gOutputComponentsMap[i] = ePixelComponentNone;
    }

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    // say we can support multiple pixel depths on in and out
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);
    
#ifdef OFX_EXTENSIONS_NUKE
    gIsMultiPlanar = OFX::getImageEffectHostDescription()->isMultiPlanar;
    gSupportsDynamicChoices = OFX::getImageEffectHostDescription()->supportsDynamicChoices;
    if (gIsMultiPlanar && gSupportsDynamicChoices && kEnableMultiPlanar) {
        // This enables fetching different planes from the input.
        // Generally the user will read a multi-layered EXR file in the Reader node and then use the shuffle
        // to redirect the plane's channels into RGBA color plane.

        desc.setIsMultiPlanar(true);
        
        // We are pass-through in output, meaning another shuffle below could very well
        // access all planes again. Note that for multi-planar effects this is mandatory to be called
        // since default is false.
        desc.setIsPassThroughForNotProcessedPlanes(true);
    } else {
        gIsMultiPlanar = false;
        gSupportsDynamicChoices = false;
    }
#else 
    gIsMultiPlanar = false;
    gSupportsDynamicChoices = false;
#endif
}

static void
addInputChannelOtions(ChoiceParamDescriptor* outputR, InputChannelEnum def, OFX::ContextEnum context)
{
    assert(outputR->getNOptions() == eInputChannelAR);
    outputR->appendOption(kParamOutputOptionAR,kParamOutputOptionARHint);
    assert(outputR->getNOptions() == eInputChannelAG);
    outputR->appendOption(kParamOutputOptionAG,kParamOutputOptionAGHint);
    assert(outputR->getNOptions() == eInputChannelAB);
    outputR->appendOption(kParamOutputOptionAB,kParamOutputOptionABHint);
    assert(outputR->getNOptions() == eInputChannelAA);
    outputR->appendOption(kParamOutputOptionAA,kParamOutputOptionAAHint);
    assert(outputR->getNOptions() == eInputChannel0);
    outputR->appendOption(kParamOutputOption0,kParamOutputOption0Hint);
    assert(outputR->getNOptions() == eInputChannel1);
    outputR->appendOption(kParamOutputOption1,kParamOutputOption1Hint);
    if (context == eContextGeneral) {
        assert(outputR->getNOptions() == eInputChannelBR);
        outputR->appendOption(kParamOutputOptionBR,kParamOutputOptionBRHint);
        assert(outputR->getNOptions() == eInputChannelBG);
        outputR->appendOption(kParamOutputOptionBG,kParamOutputOptionBGHint);
        assert(outputR->getNOptions() == eInputChannelBB);
        outputR->appendOption(kParamOutputOptionBB,kParamOutputOptionBBHint);
        assert(outputR->getNOptions() == eInputChannelBA);
        outputR->appendOption(kParamOutputOptionBA,kParamOutputOptionBAHint);
    }
    outputR->setDefault(def);
}

void ShufflePluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    
#ifdef OFX_EXTENSIONS_NUKE
    if (gIsMultiPlanar && !OFX::fetchSuite(kFnOfxImageEffectPlaneSuite, 2)) {
        OFX::throwHostMissingSuiteException(kFnOfxImageEffectPlaneSuite);
    }
#endif
    
    if (context == eContextGeneral) {
        ClipDescriptor* srcClipB = desc.defineClip(kClipB);
        srcClipB->addSupportedComponent(ePixelComponentRGBA);
        srcClipB->addSupportedComponent(ePixelComponentRGB);
        srcClipB->addSupportedComponent(ePixelComponentAlpha);
        srcClipB->setTemporalClipAccess(false);
        srcClipB->setSupportsTiles(kSupportsTiles);
        srcClipB->setOptional(true);

        ClipDescriptor* srcClipA = desc.defineClip(kClipA);
        srcClipA->addSupportedComponent(ePixelComponentRGBA);
        srcClipA->addSupportedComponent(ePixelComponentRGB);
        srcClipA->addSupportedComponent(ePixelComponentAlpha);
        srcClipA->setTemporalClipAccess(false);
        srcClipA->setSupportsTiles(kSupportsTiles);
        srcClipA->setOptional(false);
    } else {
        ClipDescriptor* srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
        srcClip->addSupportedComponent(ePixelComponentRGBA);
        srcClip->addSupportedComponent(ePixelComponentRGB);
        srcClip->addSupportedComponent(ePixelComponentAlpha);
        srcClip->setTemporalClipAccess(false);
        srcClip->setSupportsTiles(kSupportsTiles);
    }
    {
        // create the mandated output clip
        ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
        dstClip->addSupportedComponent(ePixelComponentRGBA);
        dstClip->addSupportedComponent(ePixelComponentRGB);
        dstClip->addSupportedComponent(ePixelComponentAlpha);
        dstClip->setSupportsTiles(kSupportsTiles);
    }

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    // outputComponents
    {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputComponents);
        param->setLabel(kParamOutputComponentsLabel);
        param->setHint(kParamOutputComponentsHint);
        // the following must be in the same order as in describe(), so that the map works
        if (gSupportsRGBA) {
            assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentRGBA);
            param->appendOption(kParamOutputComponentsOptionRGBA);
        }
        if (gSupportsRGB) {
            assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentRGB);
            param->appendOption(kParamOutputComponentsOptionRGB);
        }
        if (gSupportsAlpha) {
            assert(gOutputComponentsMap[param->getNOptions()] == ePixelComponentAlpha);
            param->appendOption(kParamOutputComponentsOptionAlpha);
        }
        param->setDefault(0);
        param->setAnimates(false);
        desc.addClipPreferencesSlaveParam(*param);
        if (page) {
            page->addChild(*param);
        }
    }

    // ouputBitDepth
    if (getImageEffectHostDescription()->supportsMultipleClipDepths) {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputBitDepth);
        param->setLabel(kParamOutputBitDepthLabel);
        param->setHint(kParamOutputBitDepthHint);
        // the following must be in the same order as in describe(), so that the map works
        if (gSupportsFloats) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthFloat);
            param->appendOption(kParamOutputBitDepthOptionFloat);
        }
        if (gSupportsShorts) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthUShort);
            param->appendOption(kParamOutputBitDepthOptionShort);
        }
        if (gSupportsBytes) {
            // coverity[check_return]
            assert(0 <= param->getNOptions() && param->getNOptions() < 4 && gOutputBitDepthMap[param->getNOptions()] == eBitDepthUByte);
            param->appendOption(kParamOutputBitDepthOptionByte);
        }
        param->setDefault(0);
        param->setAnimates(false);
#ifndef DEBUG
        // Shuffle only does linear conversion, which is useless for 8-bits and 16-bits formats.
        // Disable it for now (in the future, there may be colorspace conversion options)
        param->setIsSecret(true); // always secret
#endif
        desc.addClipPreferencesSlaveParam(*param);
        if (page) {
            page->addChild(*param);
        }
    }
    
    if (gSupportsRGB || gSupportsRGBA) {
        // outputR
        {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputR);
            param->setLabel(kParamOutputRLabel);
            param->setHint(kParamOutputRHint);
            if (!gSupportsDynamicChoices) {
                addInputChannelOtions(param, eInputChannelAR, context);
            }
            if (page) {
                page->addChild(*param);
            }
        }

        // outputG
        {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputG);
            param->setLabel(kParamOutputGLabel);
            param->setHint(kParamOutputGHint);
            if (!gSupportsDynamicChoices) {
                addInputChannelOtions(param, eInputChannelAG, context);
            }
            if (page) {
                page->addChild(*param);
            }
        }

        // outputB
        {
            ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputB);
            param->setLabel(kParamOutputBLabel);
            param->setHint(kParamOutputBHint);
            if (!gSupportsDynamicChoices) {
                addInputChannelOtions(param, eInputChannelAB, context);
            }
            if (page) {
                page->addChild(*param);
            }
        }
    }
    // ouputA
    if (gSupportsRGBA || gSupportsAlpha) {
        ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamOutputA);
        param->setLabel(kParamOutputALabel);
        param->setHint(kParamOutputAHint);
        if (!gSupportsDynamicChoices) {
            addInputChannelOtions(param, eInputChannelAA, context);
        }
        if (page) {
            page->addChild(*param);
        }
    }

    // clipInfo
    {
        PushButtonParamDescriptor *param = desc.definePushButtonParam(kParamClipInfo);
        param->setLabel(kParamClipInfoLabel);
        param->setHint(kParamClipInfoHint);
        if (page) {
            page->addChild(*param);
        }
    }
}

OFX::ImageEffect* ShufflePluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context)
{
    return new ShufflePlugin(handle, context);
}

void getShufflePluginID(OFX::PluginFactoryArray &ids)
{
    static ShufflePluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}

