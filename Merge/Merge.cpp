/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of openfx-misc <https://github.com/devernay/openfx-misc>,
 * Copyright (C) 2013-2017 INRIA
 *
 * openfx-misc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openfx-misc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openfx-misc.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

/*
 * OFX Merge plugin.
 */

#include <cmath>
#include <cstring>
#include <algorithm>
#include <bitset>
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#include <windows.h>
#endif
#ifdef DEBUG
#include <iostream>
#endif

#ifdef OFX_EXTENSIONS_NATRON
#include "ofxNatron.h"
#endif
#include "ofxsProcessing.H"
#include "ofxsMerging.h"
#include "ofxsCoords.h"
#include "ofxsMaskMix.h"
#include "ofxsMacros.h"

using namespace OFX;

OFXS_NAMESPACE_ANONYMOUS_ENTER

#define kPluginName "MergeOFX"
#define kPluginGrouping "Merge"
#define kPluginDescriptionStart \
    "Pixel-by-pixel merge operation between two or more inputs.\n" \
    "Input A is first merged with B (or with a black and transparent background if B is not connected), then A2, if connected, is merged with the intermediary result, then A3, etc.\n\n" \
    "A complete explanation of the Porter-Duff compositing operators can be found in \"Compositing Digital Images\", by T. Porter and T. Duff (Proc. SIGGRAPH 1984) http://keithp.com/~keithp/porterduff/p253-porter.pdf\n" \
    "\n"
#define kPluginDescriptionStartRoto \
    "Pixel-by-pixel merge operation between two inputs using and external alpha component for input A.\n" \
    "All channels from input A arge merged with those from B, using RotoMask as the alpha component for input A: the alpha channel from A is thus merged onto the alpha channel from B using the RotoMask as the alpha value (\"a\" in the formulas).\n" \
    "This may be useful, for example, to \"paint\" alpha values from A onto the alpha channel of B using a given operation with an external alpha mask (which may be opaque even where the alpha channel of A is zero).\n\n" \
    "A complete explanation of the Porter-Duff compositing operators can be found in \"Compositing Digital Images\", by T. Porter and T. Duff (Proc. SIGGRAPH 1984) http://keithp.com/~keithp/porterduff/p253-porter.pdf\n" \
    "\n"
#define kPluginDescriptionMidRGB \
    "Note that if an input with only RGB components is connected to A or B, its alpha channel " \
    "is considered to be transparent (zero) by default, and the \"A\" checkbox for the given " \
    "input is automatically unchecked, unless it is set explicitely by the user.  In fact, " \
    "most of the time, RGB images without an alpha channel are only used as background images " \
    "in the B input, and should be considered as transparent, since they should not occlude " \
    "anything. That way, the alpha channel on output only contains the opacity of elements " \
    "that are merged with this background.  In some rare cases, though, one may want the RGB " \
    "image to actually be opaque, and can check the \"A\" checkbox for the given input to do " \
    "so.\n" \
    "\n"
#define kPluginDescriptionEnd \
    "See also:\n" \
    "\n" \
    "- \"Digital Image Compositing\" by Marc Levoy https://graphics.stanford.edu/courses/cs248-06/comp/comp.html\n" \
    "- \"SVG Compositing Specification\" https://www.w3.org/TR/SVGCompositing/\n" \
    "- \"ISO 32000-1:2008: Portable Document Format (July 2008)\", Sec. 11.3 \"Basic Compositing Operations\"  http://www.adobe.com/devnet/pdf/pdf_reference.html\n" \
    "- \"Merge\" by Martin Constable http://opticalenquiry.com/nuke/index.php?title=Merge\n" \
    "- \"Merge Blend Modes\" by Martin Constable http://opticalenquiry.com/nuke/index.php?title=Merge_Blend_Modes\n" \
    "- \"Primacy of the B Feed\" by Martin Constable http://opticalenquiry.com/nuke/index.php?title=Primacy_of_the_B_Feed\n" \
    "- grain-extract and grain-merge are described in http://docs.gimp.org/en/gimp-concepts-layer-modes.html"

// merge plugin // default merge function
enum MergePluginEnum
{
    eMergePluginMerge, // eMergeOver
    eMergePluginPlus, // eMergePlus
    eMergePluginMatte, // eMergeMatte
    eMergePluginMultiply, // eMergeMultiply
    eMergePluginIn, // eMergeIn
    eMergePluginOut, // eMergeOut
    eMergePluginScreen, // eMergeScreen
    eMergePluginMax, // eMergeMax
    eMergePluginMin, // eMergeMin
    eMergePluginAbsMinus, // eMergeDifference
    eMergePluginRoto, // eMergeOver
};

#define kPluginGroupingSub "Merge/Merges"

#define kPluginNamePlus "PlusOFX"
#define kPluginNameMatte "MatteOFX"
#define kPluginNameMultiply "MultiplyOFX"
#define kPluginNameIn "InOFX"
#define kPluginNameOut "OutOFX"
#define kPluginNameScreen "ScreenOFX"
#define kPluginNameMax "MaxOFX"
#define kPluginNameMin "MinOFX"
#define kPluginNameAbsminus "AbsminusOFX"
#define kPluginNameRoto "RotoMerge"

#define kPluginIdentifier "net.sf.openfx.MergePlugin"
#define kPluginIdentifierSub "net.sf.openfx.Merge"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kParamOperation "operation"
#define kParamOperationLabel "Operation"
#define kParamOperationHint \
    "The operation used to merge the input A and B images.\n" \
    "The operator formula is applied to each component: A and B represent the input component (Red, Green, Blue, or Alpha) of each input, and a and b represent the Alpha component of each input.\n" \
    "If Alpha masking is checked, the output alpha is computed using a different formula (a+b - a*b).\n" \
    "Alpha masking is always enabled for HSL modes (hue, saturation, color, luminosity)."

#define kParamAlphaMasking "screenAlpha"
#define kParamAlphaMaskingLabel "Alpha masking"
#define kParamAlphaMaskingHint "When enabled, the input images are unchanged where the other image has 0 alpha, and" \
    " the output alpha is set to a+b - a*b. When disabled the alpha channel is processed as " \
    "any other channel. Option is disabled for operations where it does not apply or makes no difference."

#define kParamBBox "bbox"
#define kParamBBoxLabel "Bounding Box"
#define kParamBBoxHint "What to use to produce the output image's bounding box."
#define kParamBBoxOptionUnion "Union"
#define kParamBBoxOptionUnionHint "Union of all connected inputs."
#define kParamBBoxOptionIntersection "Intersection"
#define kParamBBoxOptionIntersectionHint "Intersection of all connected inputs."
#define kParamBBoxOptionA "A"
#define kParamBBoxOptionAHint "Bounding box of input A."
#define kParamBBoxOptionB "B"
#define kParamBBoxOptionBHint "Bounding box of input B."

enum BBoxEnum
{
    eBBoxUnion = 0,
    eBBoxIntersection,
    eBBoxA,
    eBBoxB
};

#define kParamAChannels       "AChannels"
#define kParamAChannelsLabel  "A Channels"
#define kParamAChannelsHint   "Channels to use from A input(s) (other channels are set to zero)."
#define kParamAChannelsR      "AChannelsR"
#define kParamAChannelsRLabel "R"
#define kParamAChannelsRHint  "Use red component from A input(s)."
#define kParamAChannelsG      "AChannelsG"
#define kParamAChannelsGLabel "G"
#define kParamAChannelsGHint  "Use green component from A input(s)."
#define kParamAChannelsB      "AChannelsB"
#define kParamAChannelsBLabel "B"
#define kParamAChannelsBHint  "Use blue component from A input(s)."
#define kParamAChannelsA      "AChannelsA"
#define kParamAChannelsALabel "A"
#define kParamAChannelsAHint  "Use alpha component from A input(s)."

#define kParamBChannels       "BChannels"
#define kParamBChannelsLabel  "B Channels"
#define kParamBChannelsHint   "Channels to use from B input (other channels are set to zero)."
#define kParamBChannelsR      "BChannelsR"
#define kParamBChannelsRLabel "R"
#define kParamBChannelsRHint  "Use red component from B input."
#define kParamBChannelsG      "BChannelsG"
#define kParamBChannelsGLabel "G"
#define kParamBChannelsGHint  "Use green component from B input."
#define kParamBChannelsB      "BChannelsB"
#define kParamBChannelsBLabel "B"
#define kParamBChannelsBHint  "Use blue component from B input."
#define kParamBChannelsA      "BChannelsA"
#define kParamBChannelsALabel "A"
#define kParamBChannelsAHint  "Use alpha component from B input."

#define kParamOutputChannels       "OutputChannels"
#define kParamOutputChannelsLabel  "Output"
#define kParamOutputChannelsHint   "Channels from result to write to output (other channels are taken from B input)."
#define kParamOutputChannelsR      "OutputChannelsR"
#define kParamOutputChannelsRLabel "R"
#define kParamOutputChannelsRHint  "Write red component to output."
#define kParamOutputChannelsG      "OutputChannelsG"
#define kParamOutputChannelsGLabel "G"
#define kParamOutputChannelsGHint  "Write green component to output."
#define kParamOutputChannelsB      "OutputChannelsB"
#define kParamOutputChannelsBLabel "B"
#define kParamOutputChannelsBHint  "Write blue component to output."
#define kParamOutputChannelsA      "OutputChannelsA"
#define kParamOutputChannelsALabel "A"
#define kParamOutputChannelsAHint  "Write alpha component to output."

#define kParamAChannelsAChanged "aChannelsChanged" // did the user explicitely change the "A" checkbox for A input?
#define kParamBChannelsAChanged "bChannelsChanged" // did the user explicitely change the "A" checkbox for B input?

#define kClipA "A"
#define kClipAHint "The image sequence to merge with input B."
#define kClipB "B"
#define kClipBHint "The main input. This input is passed through when the merge node is disabled."
#define kClipRotoMask "RotoMask"
#define kClipRotoMaskHint "A roto mask, which is used as the alpha channel in the merge operations."

#define kMaximumAInputs 64

using namespace MergeImages2D;

static
MergingFunctionEnum
getDefaultOperation(MergePluginEnum e)
{
    switch(e) {
        case eMergePluginMerge:
            return eMergeOver;
        case eMergePluginPlus:
            return eMergePlus;
        case eMergePluginMatte:
            return eMergeMatte;
        case eMergePluginMultiply:
            return eMergeMultiply;
        case eMergePluginIn:
            return eMergeIn;
        case eMergePluginOut:
            return eMergeOut;
        case eMergePluginScreen:
            return eMergeScreen;
        case eMergePluginMax:
            return eMergeMax;
        case eMergePluginMin:
            return eMergeMin;
        case eMergePluginAbsMinus:
            return eMergeDifference;
        case eMergePluginRoto:
            return eMergeOver;
    }
    return eMergeOver;
}

static
std::string
unsignedToString(unsigned i)
{
    if (i == 0) {
        return "0";
    }
    std::string nb;
    for (unsigned j = i; j != 0; j /= 10) {
        nb = (char)( '0' + (j % 10) ) + nb;
    }

    return nb;
}

/*
   For explanations on why we use bitset instead of vector<bool>, see:

   D. Kalev. What You Should Know about vector<bool>.
   http://www.informit.com/guides/content.aspx?g=cplusplus&seqNum=98

   S. D. Meyers. Effective STL: 50 Specific Ways to Improve Your Use of the Standard Template Library.
   Item 18: "Avoid using vector<bool>".
   Professional Computing Series. Addison-Wesley, Boston, 4 edition, 2004

   V. Pieterse et al. Performance of C++ Bit-vector Implementations
   http://www.cs.up.ac.za/cs/vpieterse/pub/PieterseEtAl_SAICSIT2010.pdf
 */

class MergeProcessorBase
    : public ImageProcessor
{
protected:
    const Image *_srcImgA;
    const Image *_srcImgB;
    const Image *_maskImg;
    const Image *_rotoMaskImg;
    std::vector<const Image*> _optionalAImages;
    bool _doMasking;
    bool _alphaMasking;
    double _mix;
    bool _maskInvert;
    std::bitset<4> _aChannels;
    std::bitset<4> _bChannels;
    std::bitset<4> _outputChannels;

public:

    MergeProcessorBase(ImageEffect &instance)
        : ImageProcessor(instance)
        , _srcImgA(0)
        , _srcImgB(0)
        , _maskImg(0)
        , _rotoMaskImg(0)
        , _doMasking(false)
        , _alphaMasking(false)
        , _mix(1.)
        , _maskInvert(false)
        , _aChannels()
        , _bChannels()
        , _outputChannels()
    {
    }

    void setSrcImg(const Image *A,
                   const Image *B,
                   const std::vector<const Image*>& optionalAImages)
    {
        _srcImgA = A;
        _srcImgB = B;
        _optionalAImages = optionalAImages;
    }

    void setMaskImg(const Image *v,
                    bool maskInvert) { _maskImg = v; _maskInvert = maskInvert; }

    void setRotoMaskImg(const Image *v) { _rotoMaskImg = v; }

    void doMasking(bool v) {_doMasking = v; }

    void setValues(bool alphaMasking,
                   double mix,
                   std::bitset<4> aChannels,
                   std::bitset<4> bChannels,
                   std::bitset<4> outputChannels)
    {
        _alphaMasking = alphaMasking;
        _mix = mix;
        assert(aChannels.size() == 4 && bChannels.size() == 4 && outputChannels.size() == 4);
        _aChannels = aChannels;
        _bChannels = bChannels;
        _outputChannels = outputChannels;
    }
};


template <MergingFunctionEnum f, class PIX, int nComponents, int maxValue>
class MergeProcessor
    : public MergeProcessorBase
{
public:
    MergeProcessor(ImageEffect &instance)
        : MergeProcessorBase(instance)
    {
    }

private:
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float tmpPix[4];
        float tmpA[4];
        float tmpB[4];

        for (int c = 0; c < 4; ++c) {
            tmpA[c] = tmpB[c] = 0.;
        }
        for (int y = procWindow.y1; y < procWindow.y2; ++y) {
            if ( _effect.abort() ) {
                break;
            }

            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; ++x) {
                const PIX *srcPixA = (const PIX *)  (_srcImgA ? _srcImgA->getPixelAddress(x, y) : 0);
                const PIX *srcPixB = (const PIX *)  (_srcImgB ? _srcImgB->getPixelAddress(x, y) : 0);


                if (srcPixA || srcPixB) {
                    for (std::size_t c = 0; c < nComponents; ++c) {
#                     ifdef DEBUG
                        // check for NaN
                        assert(!srcPixA || srcPixA[c] == srcPixA[c]);
                        assert(!srcPixB || srcPixB[c] == srcPixB[c]);
#                     endif
                        // all images are supposed to be black and transparent outside o
                        tmpA[c] = (_aChannels[c] && srcPixA) ? ( (float)srcPixA[c] / maxValue ) : 0.f;
                        tmpB[c] = (_bChannels[c] && srcPixB) ? ( (float)srcPixB[c] / maxValue ) : 0.f;
#                     ifdef DEBUG
                        // check for NaN
                        assert(tmpA[c] == tmpA[c]);
                        assert(tmpB[c] == tmpB[c]);
#                     endif
                    }
                    if (nComponents != 4) {
                        // set alpha (1 inside, 0 outside)
                        tmpA[3] = (_aChannels[3] && srcPixA) ? 1. : 0.;
                        tmpB[3] = (_bChannels[3] && srcPixB) ? 1. : 0.;
                    }
                    // work in float: clamping is done when mixing
                    float a;
                    if (_rotoMaskImg) {
                        const PIX *rotoMaskPix = (const PIX *)_rotoMaskImg->getPixelAddress(x, y);
                        if (rotoMaskPix) {
                            a = *rotoMaskPix;
                        } else {
                            a = 0.;
                        }
                    } else {
                        a = tmpA[3];
                    }
                    float b = tmpB[3];
                    mergePixel<f, float, 4, 1>(_alphaMasking, tmpA, a, tmpB, b, tmpPix);
                } else {
                    // everything is black and transparent
                    for (int c = 0; c < 4; ++c) {
                        tmpPix[c] = 0;
                    }
                }

#             ifdef DEBUG
                // check for NaN
                for (int c = 0; c < 4; ++c) {
                    assert(tmpPix[c] == tmpPix[c]);
                }
#             endif

                for (std::size_t i = 0; i < _optionalAImages.size(); ++i) {
                    srcPixA = (const PIX *)  (_optionalAImages[i] ? _optionalAImages[i]->getPixelAddress(x, y) : 0);

                    if (srcPixA) {
                        for (std::size_t c = 0; c < nComponents; ++c) {
#                     ifdef DEBUG
                            // check for NaN
                            assert(srcPixA[c] == srcPixA[c]);
#                     endif
                            // all images are supposed to be black and transparent outside o
                            tmpA[c] = _aChannels[c] ? ( (float)srcPixA[c] / maxValue ) : 0.f;
#                         ifdef DEBUG
                            // check for NaN
                            assert(tmpA[c] == tmpA[c]);
#                         endif
                        }
                        if (nComponents != 4) {
                            // set alpha (1 inside, 0 outside)
                            assert(srcPixA);
                            tmpA[3] = _aChannels[3] ? 1. : 0.;
                        }

                        // work in float: clamping is done when mixing
                        float a = tmpA[3];
                        float b = tmpPix[3];
                        mergePixel<f, float, nComponents, 1>(_alphaMasking, tmpA, a, tmpPix, b, tmpPix);

#                     ifdef DEBUG
                        // check for NaN
                        for (int c = 0; c < 4; ++c) {
                            assert(tmpPix[c] == tmpPix[c]);
                        }
#                     endif
                    }
                }

                // tmpPix has 4 components, but we only need the first nComponents

                // denormalize
                for (int c = 0; c < nComponents; ++c) {
                    tmpPix[c] *= maxValue;
                }

                ofxsMaskMixPix<PIX, nComponents, maxValue, true>(tmpPix, x, y, srcPixB, _doMasking, _maskImg, (float)_mix, _maskInvert, dstPix);
                for (int c = 0; c < nComponents; ++c) {
                    if (!_outputChannels[c]) {
                        dstPix[c] = srcPixB ? srcPixB[c] : 0;
                    }
                }

                dstPix += nComponents;
            }
        }
    } // multiThreadProcessImages
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class MergePlugin
    : public ImageEffect
{
public:
    /** @brief ctor */
    MergePlugin(OfxImageEffectHandle handle,
                MergePluginEnum plugin,
                bool numerousInputs)
        : ImageEffect(handle)
        , _plugin(plugin)
        , _dstClip(0)
        , _srcClipA(0)
        , _srcClipB(0)
        , _maskClip(0)
        , _rotoMaskClip(0)
        , _optionalASrcClips(0)
        , _aChannelAChanged(0)
        , _bChannelAChanged(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert( _dstClip && (!_dstClip->isConnected() || _dstClip->getPixelComponents() == ePixelComponentRGB || _dstClip->getPixelComponents() == ePixelComponentRGBA || _dstClip->getPixelComponents() == ePixelComponentAlpha) );
        _srcClipA = fetchClip(kClipA);
        assert( _srcClipA && (!_srcClipA->isConnected() || _srcClipA->getPixelComponents() == ePixelComponentRGB || _srcClipA->getPixelComponents() == ePixelComponentRGBA || _srcClipA->getPixelComponents() == ePixelComponentAlpha) );

        if (numerousInputs) {
            _optionalASrcClips.resize(kMaximumAInputs - 1);
            for (int i = 2; i <= kMaximumAInputs; ++i) {
                Clip* clip = fetchClip( std::string(kClipA) + unsignedToString(i) );
                assert( clip && (!clip->isConnected() || clip->getPixelComponents() == ePixelComponentRGB || clip->getPixelComponents() == ePixelComponentRGBA || clip->getPixelComponents() == ePixelComponentAlpha) );
                _optionalASrcClips[i - 2] = clip;
            }
        }

        _srcClipB = fetchClip(kClipB);
        assert( _srcClipB && (!_srcClipB->isConnected() || _srcClipB->getPixelComponents() == ePixelComponentRGB || _srcClipB->getPixelComponents() == ePixelComponentRGBA || _srcClipB->getPixelComponents() == ePixelComponentAlpha) );
        _maskClip = fetchClip(getContext() == eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || !_maskClip->isConnected() || _maskClip->getPixelComponents() == ePixelComponentAlpha);

        if (plugin == eMergePluginRoto) {
            _rotoMaskClip = fetchClip(kClipRotoMask);
            assert(!_rotoMaskClip || !_rotoMaskClip->isConnected() || _rotoMaskClip->getPixelComponents() == ePixelComponentAlpha);
        }
        _operation = fetchChoiceParam(kParamOperation);
        _operationString = fetchStringParam(kNatronOfxParamStringSublabelName);
        _bbox = fetchChoiceParam(kParamBBox);
        _alphaMasking = fetchBooleanParam(kParamAlphaMasking);
        assert(_operation && _operationString && _bbox && _alphaMasking);
        _mix = fetchDoubleParam(kParamMix);
        _maskApply = paramExists(kParamMaskApply) ? fetchBooleanParam(kParamMaskApply) : 0;
        _maskInvert = fetchBooleanParam(kParamMaskInvert);
        assert(_mix && _maskInvert);

        _aChannels[0] = fetchBooleanParam(kParamAChannelsR);
        _aChannels[1] = fetchBooleanParam(kParamAChannelsG);
        _aChannels[2] = fetchBooleanParam(kParamAChannelsB);
        _aChannels[3] = fetchBooleanParam(kParamAChannelsA);
        assert(_aChannels[0] && _aChannels[1] && _aChannels[2] && _aChannels[3]);

        _bChannels[0] = fetchBooleanParam(kParamBChannelsR);
        _bChannels[1] = fetchBooleanParam(kParamBChannelsG);
        _bChannels[2] = fetchBooleanParam(kParamBChannelsB);
        _bChannels[3] = fetchBooleanParam(kParamBChannelsA);
        assert(_bChannels[0] && _bChannels[1] && _bChannels[2] && _bChannels[3]);

        _outputChannels[0] = fetchBooleanParam(kParamOutputChannelsR);
        _outputChannels[1] = fetchBooleanParam(kParamOutputChannelsG);
        _outputChannels[2] = fetchBooleanParam(kParamOutputChannelsB);
        _outputChannels[3] = fetchBooleanParam(kParamOutputChannelsA);
        assert(_outputChannels[0] && _outputChannels[1] && _outputChannels[2] && _outputChannels[3]);

        if ( getImageEffectHostDescription()->supportsPixelComponent(ePixelComponentRGB) ) {
            _aChannelAChanged = fetchBooleanParam(kParamAChannelsAChanged);
            _bChannelAChanged = fetchBooleanParam(kParamBChannelsAChanged);
        }
    }

private:
    // override the rod call
    virtual bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const RenderArguments &args) OVERRIDE FINAL;
    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    /* set up and run a processor */
    void setupAndProcess(MergeProcessorBase &, const RenderArguments &args);

    virtual void changedClip(const InstanceChangedArgs &args, const std::string &clipName) OVERRIDE FINAL;
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime) OVERRIDE FINAL;
    virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

private:
    template<int nComponents>
    void renderForComponents(const RenderArguments &args);

    template <class PIX, int nComponents, int maxValue>
    void renderForBitDepth(const RenderArguments &args);

    MergePluginEnum _plugin;
    // do not need to delete these, the ImageEffect is managing them for us
    Clip *_dstClip;
    Clip *_srcClipA;
    Clip *_srcClipB;
    Clip *_maskClip;
    Clip *_rotoMaskClip;
    std::vector<Clip *> _optionalASrcClips;
    ChoiceParam *_operation;
    StringParam *_operationString;
    ChoiceParam *_bbox;
    BooleanParam *_alphaMasking;
    DoubleParam* _mix;
    BooleanParam* _maskApply;
    BooleanParam* _maskInvert;
    BooleanParam* _aChannels[4];
    BooleanParam* _bChannels[4];
    BooleanParam* _outputChannels[4];
    BooleanParam* _aChannelAChanged;
    BooleanParam* _bChannelAChanged;
};


// override the rod call
bool
MergePlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args,
                                   OfxRectD &rod)
{
    const double time = args.time;
    double mix = _mix->getValueAtTime(time);

    //Do the same as isIdentity otherwise the result of getRegionOfDefinition() might not be coherent with the RoD of the identity clip.
    if (mix == 0.) {
        if ( _srcClipB->isConnected() ) {
            OfxRectD rodB = _srcClipB->getRegionOfDefinition(time);
            rod = rodB;

            return true;
        }

        return false;
    }

    std::vector<OfxRectD> rods;
    BBoxEnum bboxChoice = (BBoxEnum)_bbox->getValueAtTime(time);
    if ( (bboxChoice == eBBoxUnion) || (bboxChoice == eBBoxIntersection) ) {
        if ( _srcClipB->isConnected() ) {
            rods.push_back( _srcClipB->getRegionOfDefinition(time) );
        }
        if ( _srcClipA->isConnected() ) {
            rods.push_back( _srcClipA->getRegionOfDefinition(time) );
        }
        for (std::size_t i = 0; i < _optionalASrcClips.size(); ++i) {
            if ( _optionalASrcClips[i]->isConnected() ) {
                rods.push_back( _optionalASrcClips[i]->getRegionOfDefinition(time) );
            }
        }
        if ( !rods.size() ) {
            return false;
        }
        rod = rods[0];
    }
    switch (bboxChoice) {
    case eBBoxUnion: {     //union
        for (unsigned i = 1; i < rods.size(); ++i) {
            Coords::rectBoundingBox(rod, rods[i], &rod);
        }

        return true;
    }
    case eBBoxIntersection: {     //intersection
        for (unsigned i = 1; i < rods.size(); ++i) {
            Coords::rectIntersection(rod, rods[i], &rod);
        }

        // may return an empty RoD if intersection is empty
        return true;
    }
    case eBBoxA: {     //A
        if ( _srcClipA->isConnected() ) {
            rod = _srcClipA->getRegionOfDefinition(time);

            return true;
        }

        return false;
    }
    case eBBoxB: {     //B
        if ( _srcClipB->isConnected() ) {
            rod = _srcClipB->getRegionOfDefinition(time);

            return true;
        }

        return false;
    }
    }

    return false;
} // MergePlugin::getRegionOfDefinition

// Since we cannot hold a std::auto_ptr in the vector we must hold a raw pointer.
// To ensure that images are always freed even in case of exceptions, use a RAII class.
struct OptionalImagesHolder_RAII
{
    std::vector<const Image*> images;

    OptionalImagesHolder_RAII()
        : images()
    {
    }

    ~OptionalImagesHolder_RAII()
    {
        for (std::size_t i = 0; i < images.size(); ++i) {
            delete images[i];
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

/* set up and run a processor */
void
MergePlugin::setupAndProcess(MergeProcessorBase &processor,
                             const RenderArguments &args)
{
    const double time = args.time;

    std::auto_ptr<Image> dst( _dstClip->fetchImage(time) );

    if ( !dst.get() ) {
        throwSuiteStatusException(kOfxStatFailed);
    }
    BitDepthEnum dstBitDepth    = dst->getPixelDepth();
    PixelComponentEnum dstComponents  = dst->getPixelComponents();
    if ( ( dstBitDepth != _dstClip->getPixelDepth() ) ||
         ( dstComponents != _dstClip->getPixelComponents() ) ) {
        setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        throwSuiteStatusException(kOfxStatFailed);
    }
    if ( (dst->getRenderScale().x != args.renderScale.x) ||
         ( dst->getRenderScale().y != args.renderScale.y) ||
         ( ( dst->getField() != eFieldNone) /* for DaVinci Resolve */ && ( dst->getField() != args.fieldToRender) ) ) {
        setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        throwSuiteStatusException(kOfxStatFailed);
    }
    std::auto_ptr<const Image> srcA( ( _srcClipA && _srcClipA->isConnected() ) ?
                                     _srcClipA->fetchImage(time) : 0 );
    std::auto_ptr<const Image> srcB( ( _srcClipB && _srcClipB->isConnected() ) ?
                                     _srcClipB->fetchImage(time) : 0 );
    OptionalImagesHolder_RAII optionalImages;
    for (std::size_t i = 0; i < _optionalASrcClips.size(); ++i) {
        const Image* optImg = ( _optionalASrcClips[i] && _optionalASrcClips[i]->isConnected() ) ?
                              _optionalASrcClips[i]->fetchImage(time) : 0;
        if (optImg) {
            optionalImages.images.push_back(optImg);
        }

        if (optImg) {
            if ( (optImg->getRenderScale().x != args.renderScale.x) ||
                 ( optImg->getRenderScale().y != args.renderScale.y) ||
                 ( ( optImg->getField() != eFieldNone) /* for DaVinci Resolve */ && ( optImg->getField() != args.fieldToRender) ) ) {
                setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
                throwSuiteStatusException(kOfxStatFailed);
            }
            BitDepthEnum srcBitDepth      = optImg->getPixelDepth();
            PixelComponentEnum srcComponents = optImg->getPixelComponents();
            if ( (srcBitDepth != dstBitDepth) || (srcComponents != dstComponents) ) {
                throwSuiteStatusException(kOfxStatErrImageFormat);
            }
        }
    }

    if ( srcA.get() ) {
        if ( (srcA->getRenderScale().x != args.renderScale.x) ||
             ( srcA->getRenderScale().y != args.renderScale.y) ||
             ( ( srcA->getField() != eFieldNone) /* for DaVinci Resolve */ && ( srcA->getField() != args.fieldToRender) ) ) {
            setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            throwSuiteStatusException(kOfxStatFailed);
        }
        BitDepthEnum srcBitDepth      = srcA->getPixelDepth();
        PixelComponentEnum srcComponents = srcA->getPixelComponents();
        if ( (srcBitDepth != dstBitDepth) || (srcComponents != dstComponents) ) {
            throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }

    if ( srcB.get() ) {
        if ( (srcB->getRenderScale().x != args.renderScale.x) ||
             ( srcB->getRenderScale().y != args.renderScale.y) ||
             ( ( srcB->getField() != eFieldNone) /* for DaVinci Resolve */ && ( srcB->getField() != args.fieldToRender) ) ) {
            setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
            throwSuiteStatusException(kOfxStatFailed);
        }
        BitDepthEnum srcBitDepth      = srcB->getPixelDepth();
        PixelComponentEnum srcComponents = srcB->getPixelComponents();
        if ( (srcBitDepth != dstBitDepth) || (srcComponents != dstComponents) ) {
            throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }

    // auto ptr for the mask.
    bool doMasking = ( ( !_maskApply || _maskApply->getValueAtTime(time) ) && _maskClip && _maskClip->isConnected() );
    std::auto_ptr<const Image> mask(doMasking ? _maskClip->fetchImage(time) : 0);

    // do we do masking
    if (doMasking) {
        bool maskInvert = _maskInvert->getValueAtTime(time);

        // say we are masking
        processor.doMasking(true);

        // Set it in the processor
        processor.setMaskImg(mask.get(), maskInvert);
    }

    std::auto_ptr<const Image> rotoMask(_rotoMaskClip ? _rotoMaskClip->fetchImage(time) : 0);
    processor.setRotoMaskImg(rotoMask.get());

    bool alphaMasking = _alphaMasking->getValueAtTime(time);
    double mix = _mix->getValueAtTime(time);
    std::bitset<4> aChannels;
    std::bitset<4> bChannels;
    std::bitset<4> outputChannels;
    for (std::size_t c = 0; c < 4; ++c) {
        aChannels[c] = _aChannels[c]->getValueAtTime(time);
        bChannels[c] = _bChannels[c]->getValueAtTime(time);
        outputChannels[c] = _outputChannels[c]->getValueAtTime(time);
    }
    processor.setValues(alphaMasking, mix, aChannels, bChannels, outputChannels);
    processor.setDstImg( dst.get() );
    processor.setSrcImg(srcA.get(), srcB.get(), optionalImages.images);
    processor.setRenderWindow(args.renderWindow);

    processor.process();
} // MergePlugin::setupAndProcess

template<int nComponents>
void
MergePlugin::renderForComponents(const RenderArguments &args)
{
    BitDepthEnum dstBitDepth    = _dstClip->getPixelDepth();

    switch (dstBitDepth) {
    case eBitDepthUByte: {
        renderForBitDepth<unsigned char, nComponents, 255>(args);
        break;
    }
    case eBitDepthUShort: {
        renderForBitDepth<unsigned short, nComponents, 65535>(args);
        break;
    }
    case eBitDepthFloat: {
        renderForBitDepth<float, nComponents, 1>(args);
        break;
    }
    default:
        throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

template <class PIX, int nComponents, int maxValue>
void
MergePlugin::renderForBitDepth(const RenderArguments &args)
{
    const double time = args.time;
    MergingFunctionEnum operation = (MergingFunctionEnum)_operation->getValueAtTime(time);

    std::auto_ptr<MergeProcessorBase> fred;

    switch (operation) {
    case eMergeATop:
        fred.reset( new MergeProcessor<eMergeATop, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeAverage:
        fred.reset( new MergeProcessor<eMergeAverage, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeColorBurn:
        fred.reset( new MergeProcessor<eMergeColorBurn, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeColorDodge:
        fred.reset( new MergeProcessor<eMergeColorDodge, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeConjointOver:
        fred.reset( new MergeProcessor<eMergeConjointOver, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeCopy:
        fred.reset( new MergeProcessor<eMergeCopy, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeDifference:
        fred.reset( new MergeProcessor<eMergeDifference, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeDisjointOver:
        fred.reset( new MergeProcessor<eMergeDisjointOver, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeDivide:
        fred.reset( new MergeProcessor<eMergeDivide, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeExclusion:
        fred.reset( new MergeProcessor<eMergeExclusion, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeFreeze:
        fred.reset( new MergeProcessor<eMergeFreeze, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeFrom:
        fred.reset( new MergeProcessor<eMergeFrom, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeGeometric:
        fred.reset( new MergeProcessor<eMergeGeometric, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeGrainExtract:
        fred.reset( new MergeProcessor<eMergeGrainExtract, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeGrainMerge:
        fred.reset( new MergeProcessor<eMergeGrainMerge, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeHardLight:
        fred.reset( new MergeProcessor<eMergeHardLight, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeHypot:
        fred.reset( new MergeProcessor<eMergeHypot, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeIn:
        fred.reset( new MergeProcessor<eMergeIn, PIX, nComponents, maxValue>(*this) );
        break;
    //case eMergeInterpolated:
    //    fred.reset(new MergeProcessor<eMergeInterpolated, PIX, nComponents, maxValue>(*this));
    //    break;
    case eMergeMask:
        fred.reset( new MergeProcessor<eMergeMask, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeMatte:
        fred.reset( new MergeProcessor<eMergeMatte, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeMax:
        fred.reset( new MergeProcessor<eMergeMax, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeMin:
        fred.reset( new MergeProcessor<eMergeMin, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeMinus:
        fred.reset( new MergeProcessor<eMergeMinus, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeMultiply:
        fred.reset( new MergeProcessor<eMergeMultiply, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeOut:
        fred.reset( new MergeProcessor<eMergeOut, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeOver:
        fred.reset( new MergeProcessor<eMergeOver, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeOverlay:
        fred.reset( new MergeProcessor<eMergeOverlay, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergePinLight:
        fred.reset( new MergeProcessor<eMergePinLight, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergePlus:
        fred.reset( new MergeProcessor<eMergePlus, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeReflect:
        fred.reset( new MergeProcessor<eMergeReflect, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeScreen:
        fred.reset( new MergeProcessor<eMergeScreen, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeSoftLight:
        fred.reset( new MergeProcessor<eMergeSoftLight, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeStencil:
        fred.reset( new MergeProcessor<eMergeStencil, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeUnder:
        fred.reset( new MergeProcessor<eMergeUnder, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeXOR:
        fred.reset( new MergeProcessor<eMergeXOR, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeHue:
        fred.reset( new MergeProcessor<eMergeHue, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeSaturation:
        fred.reset( new MergeProcessor<eMergeSaturation, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeColor:
        fred.reset( new MergeProcessor<eMergeColor, PIX, nComponents, maxValue>(*this) );
        break;
    case eMergeLuminosity:
        fred.reset( new MergeProcessor<eMergeLuminosity, PIX, nComponents, maxValue>(*this) );
        break;
    } // switch
    assert( fred.get() );
    if ( fred.get() ) {
        setupAndProcess(*fred, args);
    }
} // MergePlugin::renderForBitDepth

// the overridden render function
void
MergePlugin::render(const RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert( kSupportsMultipleClipPARs   || _srcClipA->getPixelAspectRatio() == _dstClip->getPixelAspectRatio() );
    assert( kSupportsMultipleClipDepths || _srcClipA->getPixelDepth()       == _dstClip->getPixelDepth() );
    assert( kSupportsMultipleClipPARs   || _srcClipB->getPixelAspectRatio() == _dstClip->getPixelAspectRatio() );
    assert( kSupportsMultipleClipDepths || _srcClipB->getPixelDepth()       == _dstClip->getPixelDepth() );
    if (dstComponents == ePixelComponentRGBA) {
        renderForComponents<4>(args);
    } else if (dstComponents == ePixelComponentRGB) {
        renderForComponents<3>(args);
    } else if (dstComponents == ePixelComponentXY) {
        renderForComponents<2>(args);
    } else {
        assert(dstComponents == ePixelComponentAlpha);
        renderForComponents<1>(args);
    }
}

void
MergePlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences)
{
    PixelComponentEnum outputComps = _dstClip->getPixelComponents();

    clipPreferences.setClipComponents(*_srcClipA, outputComps);
    clipPreferences.setClipComponents(*_srcClipB, outputComps);
    for (std::size_t i = 0; i < _optionalASrcClips.size(); ++i) {
        clipPreferences.setClipComponents(*_optionalASrcClips[i], outputComps);
    }
}

void
MergePlugin::changedParam(const InstanceChangedArgs &args,
                          const std::string &paramName)
{
    if (paramName == kParamOperation) {
        MergingFunctionEnum operation = (MergingFunctionEnum)_operation->getValueAtTime(args.time);
        // depending on the operation, enable/disable alpha masking
        _alphaMasking->setEnabled( MergeImages2D::isMaskable(operation) );
        _operationString->setValue( MergeImages2D::getOperationString(operation) );
    } else if ( _aChannelAChanged && (paramName == kParamAChannelsA) && (args.reason == eChangeUserEdit) ) {
        _aChannelAChanged->setValue(true);
    } else if ( _bChannelAChanged && (paramName == kParamBChannelsA) && (args.reason == eChangeUserEdit) ) {
        _bChannelAChanged->setValue(true);
    }
}

void
MergePlugin::changedClip(const InstanceChangedArgs &args,
                         const std::string &clipName)
{
    if ( _bChannelAChanged && !_bChannelAChanged->getValue() && (clipName == kClipB) && _srcClipB && _srcClipB->isConnected() && ( args.reason == eChangeUserEdit) ) {
        PixelComponentEnum unmappedComps = _srcClipB->getUnmappedPixelComponents();
        // If A is RGBA and B is RGB, getClipPreferences will remap B to RGBA.
        // If before the clip preferences pass the input is RGB then don't consider the alpha channel for the clip B and use 0 instead.
        if (unmappedComps == ePixelComponentRGB) {
            _bChannels[3]->setValue(false);
        }
        if ( (unmappedComps == ePixelComponentRGBA) || (unmappedComps == ePixelComponentAlpha) ) {
            _bChannels[3]->setValue(true);
        }
    } else if ( _aChannelAChanged && !_aChannelAChanged->getValue() && (clipName == kClipA) && _srcClipA && _srcClipA->isConnected() && ( args.reason == eChangeUserEdit) ) {
        // Note: we do not care about clips A2, A3, ...
        PixelComponentEnum unmappedComps = _srcClipA->getUnmappedPixelComponents();
        // If A is RGBA and B is RGB, getClipPreferences will remap B to RGBA.
        // If before the clip preferences pass the input is RGB then don't consider the alpha channel for the clip B and use 0 instead.
        if (unmappedComps == ePixelComponentRGB) {
            _aChannels[3]->setValue(false);
        }
        if ( (unmappedComps == ePixelComponentRGBA) || (unmappedComps == ePixelComponentAlpha) ) {
            _aChannels[3]->setValue(true);
        }
    }
}

bool
MergePlugin::isIdentity(const IsIdentityArguments &args,
                        Clip * &identityClip,
                        double & /*identityTime*/)
{
    const double time = args.time;
    double mix;

    _mix->getValueAtTime(time, mix);

    if (mix == 0.) {
        identityClip = _srcClipB;

        return true;
    }

    bool outputChannels[4];
    for (int c = 0; c < 4; ++c) {
        _outputChannels[c]->getValueAtTime(time, outputChannels[c]);
    }
    if (!outputChannels[0] && !outputChannels[1] && !outputChannels[2] && !outputChannels[3]) {
        identityClip = _srcClipB;

        return true;
    }

    OfxRectI maskRoD;
    bool maskRoDValid = false;
    bool doMasking = ( ( !_maskApply || _maskApply->getValueAtTime(time) ) && _maskClip && _maskClip->isConnected() );
    if (doMasking) {
        bool maskInvert;
        _maskInvert->getValueAtTime(time, maskInvert);
        if (!maskInvert) {
            maskRoDValid = true;
            Coords::toPixelEnclosing(_maskClip->getRegionOfDefinition(time), args.renderScale, _maskClip->getPixelAspectRatio(), &maskRoD);
            // effect is identity if the renderWindow doesn't intersect the mask RoD
            if ( !Coords::rectIntersection<OfxRectI>(args.renderWindow, maskRoD, 0) ) {
                identityClip = _srcClipB;

                return true;
            }
        }
    }

    // The region of effect is only the set of the intersections between the A inputs and the mask.
    // If at least one of these regions intersects the renderwindow, the effect is not identity.

    std::vector<Clip *> aClips = _optionalASrcClips;
    aClips.push_back(_srcClipA);
    for (std::size_t i = 0; i < aClips.size(); ++i) {
        if ( !aClips[i]->isConnected() ) {
            continue;
        }
        OfxRectD srcARoD = aClips[i]->getRegionOfDefinition(time);
        if ( Coords::rectIsEmpty(srcARoD) ) {
            // RoD is empty
            continue;
        }

        OfxRectI srcARoDPixel;
        Coords::toPixelEnclosing(srcARoD, args.renderScale, aClips[i]->getPixelAspectRatio(), &srcARoDPixel);
        bool srcARoDValid = true;
        if (maskRoDValid) {
            // mask the srcARoD with the mask RoD. The result may be empty
            srcARoDValid = Coords::rectIntersection<OfxRectI>(srcARoDPixel, maskRoD, &srcARoDPixel);
        }
        if ( srcARoDValid && Coords::rectIntersection<OfxRectI>(args.renderWindow, srcARoDPixel, 0) ) {
            // renderWindow intersects one of the effect areas
            return false;
        }
    }

    // renderWindow intersects no area where a "A" source is applied
    identityClip = _srcClipB;

    return true;
} // MergePlugin::isIdentity

//mDeclarePluginFactory(MergePluginFactory, {}, {});
template<MergePluginEnum plugin>
class MergePluginFactory
    : public PluginFactoryHelper<MergePluginFactory<plugin> >
{
public:
    MergePluginFactory<plugin>(const std::string & id, unsigned int verMaj, unsigned int verMin)
    : PluginFactoryHelper<MergePluginFactory>(id, verMaj, verMin)
    {
    }

    virtual void describe(ImageEffectDescriptor &desc);
    virtual void describeInContext(ImageEffectDescriptor &desc, ContextEnum context);
    virtual ImageEffect* createInstance(OfxImageEffectHandle handle, ContextEnum context);
};

template<MergePluginEnum plugin>
void
MergePluginFactory<plugin>::describe(ImageEffectDescriptor &desc)
{
    // basic labels

    switch (plugin) {
    case eMergePluginMerge:
        desc.setLabel(kPluginName);
        desc.setPluginGrouping(kPluginGrouping);
        break;
    case eMergePluginPlus:
        desc.setLabel(kPluginNamePlus);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginMatte:
        desc.setLabel(kPluginNameMatte);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginMultiply:
        desc.setLabel(kPluginNameMultiply);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginIn:
        desc.setLabel(kPluginNameIn);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginOut:
        desc.setLabel(kPluginNameOut);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginScreen:
        desc.setLabel(kPluginNameScreen);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginMax:
        desc.setLabel(kPluginNameMax);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginMin:
        desc.setLabel(kPluginNameMin);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginAbsMinus:
        desc.setLabel(kPluginNameAbsminus);
        desc.setPluginGrouping(kPluginGroupingSub);
        break;
    case eMergePluginRoto:
        desc.setLabel(kPluginNameRoto);
        desc.setPluginGrouping(kPluginGrouping);
        break;
    }
    std::string help = (plugin == eMergePluginRoto ? kPluginDescriptionStartRoto : kPluginDescriptionStart);
    if ( getImageEffectHostDescription()->supportsPixelComponent(ePixelComponentRGB) ) {
        // Merge has a special way of handling RGB inputs, which are transparent by default
        help += kPluginDescriptionMidRGB;
    }
    // only Natron benefits from the long description, because '<' characters may break the OFX
    // plugins cache in hosts using the older HostSupport library.
    if (getImageEffectHostDescription()->isNatron) {
        help += "### Operators\n";
        help += "The following operators are available.\n";
        help += "\n#### Porter-Duff compositing operators\n";
        // missing: clear
        help += "\n- " + getOperationHelp(eMergeCopy, true) + '\n'; // src
        // missing: dst
        help += "\n- " + getOperationHelp(eMergeOver, true) + '\n'; // src-over
        help += "\n- " + getOperationHelp(eMergeUnder, true) + '\n'; // dst-over
        help += "\n- " + getOperationHelp(eMergeIn, true) + '\n'; // src-in
        help += "\n- " + getOperationHelp(eMergeMask, true) + '\n'; // dst-in
        help += "\n- " + getOperationHelp(eMergeOut, true) + '\n'; // src-out
        help += "\n- " + getOperationHelp(eMergeStencil, true) + '\n'; // dst-out
        help += "\n- " + getOperationHelp(eMergeATop, true) + '\n'; // src-atop
        // missing: dst-atop
        help += "\n- " + getOperationHelp(eMergeXOR, true) + '\n'; // xor

        help += "\n#### Blend modes, see https://en.wikipedia.org/wiki/Blend_modes\n";
        help += "\n##### Multiply and Screen\n";
        help += "\n- " + getOperationHelp(eMergeMultiply, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeScreen, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeOverlay, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeHardLight, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeSoftLight, true) + '\n';
        help += "\n##### Dodge and burn\n";
        help += "\n- " + getOperationHelp(eMergeColorDodge, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeColorBurn, true) + '\n';
        help += "\n- " + getOperationHelp(eMergePinLight, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeDifference, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeExclusion, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeDivide, true) + '\n';
        help += "\n##### Simple arithmetic blend modes\n";
        help += "\n- " + getOperationHelp(eMergeDivide, true) + '\n';
        help += "\n- " + getOperationHelp(eMergePlus, true) + '\n';// add (see <http://keithp.com/~keithp/render/protocol.html>)
        help += "\n- " + getOperationHelp(eMergeFrom, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeMinus, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeDifference, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeMin, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeMax, true) + '\n';
        help += "\n##### Hue, saturation and luminosity\n";
        help += "\n- " + getOperationHelp(eMergeHue, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeSaturation, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeColor, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeLuminosity, true) + '\n';
        help += "\n#### Other\n";
        help += "\n- " + getOperationHelp(eMergeAverage, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeConjointOver, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeDisjointOver, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeFreeze, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeGeometric, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeGrainExtract, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeGrainMerge, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeHypot, true) + '\n';
        //help += "\n- " + getOperationHelp(eMergeInterpolated, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeMatte, true) + '\n';
        help += "\n- " + getOperationHelp(eMergeReflect, true) + '\n';
        help += '\n';
    }
    help += kPluginDescriptionEnd;
    if (getImageEffectHostDescription()->isNatron) {
        desc.setDescriptionIsMarkdown(true);
        desc.setPluginDescription(help, /*validate=*/ false);
    } else {
        desc.setPluginDescription(help);
    }

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);
} // >::describe

static void
addMergeOption(ChoiceParamDescriptor* param,
               MergingFunctionEnum e,
               bool cascading)
{
    assert(param->getNOptions() == e);
    if (cascading) {
        param->appendOption( getOperationGroupString(e) + '/' + getOperationString(e), getOperationDescription(e) );
    } else {
        param->appendOption( getOperationString(e), /*'(' + getOperationGroupString(e) + ") " +*/ getOperationDescription(e) );
    }
}

template<MergePluginEnum plugin>
void
MergePluginFactory<plugin>::describeInContext(ImageEffectDescriptor &desc,
                                              ContextEnum context)
{
    //Natron >= 2.0 allows multiple inputs to be folded like the viewer node, so use this to merge
    //more than 2 images
    bool numerousInputs =  (plugin != eMergePluginRoto &&
                            getImageEffectHostDescription()->isNatron &&
                            getImageEffectHostDescription()->versionMajor >= 2);
    ClipDescriptor* srcClipB = desc.defineClip(kClipB);

    srcClipB->setHint(kClipBHint);
    srcClipB->addSupportedComponent( ePixelComponentRGBA );
    srcClipB->addSupportedComponent( ePixelComponentRGB );
    srcClipB->addSupportedComponent( ePixelComponentXY );
    srcClipB->addSupportedComponent( ePixelComponentAlpha );
    srcClipB->setTemporalClipAccess(false);
    srcClipB->setSupportsTiles(kSupportsTiles);

    //Optional: If we want a render to be triggered even if one of the inputs is not connected
    //they need to be optional.
    srcClipB->setOptional(true); // B clip is optional

    ClipDescriptor* srcClipA = desc.defineClip(kClipA);
    srcClipA->setHint(kClipAHint);
    srcClipA->addSupportedComponent( ePixelComponentRGBA );
    srcClipA->addSupportedComponent( ePixelComponentRGB );
    srcClipA->addSupportedComponent( ePixelComponentXY );
    srcClipA->addSupportedComponent( ePixelComponentAlpha );
    srcClipA->setTemporalClipAccess(false);
    srcClipA->setSupportsTiles(kSupportsTiles);

    //Optional: If we want a render to be triggered even if one of the inputs is not connected
    //they need to be optional.
    srcClipA->setOptional(true);

    ClipDescriptor *maskClip = (context == eContextPaint) ? desc.defineClip("Brush") : desc.defineClip("Mask");
    maskClip->addSupportedComponent(ePixelComponentAlpha);
    maskClip->setTemporalClipAccess(false);
    if (context != eContextPaint) {
        maskClip->setOptional(true);
    }
    maskClip->setSupportsTiles(kSupportsTiles);
    maskClip->setIsMask(true);

    if (plugin == eMergePluginRoto) {
        ClipDescriptor* rotoMaskClip = desc.defineClip(kClipRotoMask);
        rotoMaskClip->addSupportedComponent(ePixelComponentAlpha);
        rotoMaskClip->setTemporalClipAccess(false);
        rotoMaskClip->setOptional(true);
        rotoMaskClip->setSupportsTiles(kSupportsTiles);
        rotoMaskClip->setIsMask(true);
    }

    if (numerousInputs) {
        for (int i = 2; i <= kMaximumAInputs; ++i) {
            ClipDescriptor* optionalSrcClip = desc.defineClip( std::string(kClipA) + unsignedToString(i) );
            optionalSrcClip->addSupportedComponent( ePixelComponentRGBA );
            optionalSrcClip->addSupportedComponent( ePixelComponentRGB );
            optionalSrcClip->addSupportedComponent( ePixelComponentXY );
            optionalSrcClip->addSupportedComponent( ePixelComponentAlpha );
            optionalSrcClip->setTemporalClipAccess(false);
            optionalSrcClip->setSupportsTiles(kSupportsTiles);

            //Optional: If we want a render to be triggered even if one of the inputs is not connected
            //they need to be optional.
            optionalSrcClip->setOptional(true);
        }
    }


    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentXY);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);


    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    // operationString
    {
        StringParamDescriptor* param = desc.defineStringParam(kNatronOfxParamStringSublabelName);
        param->setIsSecretAndDisabled(true); // always secret
        param->setIsPersistent(true);
        param->setEvaluateOnChange(false);
        param->setDefault( getOperationString( getDefaultOperation(plugin) ) );
        if (page) {
            page->addChild(*param);
        }
    }

    // operation
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamOperation);
        param->setLabel(kParamOperationLabel);
        param->setHint(kParamOperationHint);
        bool cascading = false;// getImageEffectHostDescription()->supportsCascadingChoices;
        param->setCascading(cascading);
        addMergeOption(param, eMergeATop, cascading);
        addMergeOption(param, eMergeAverage, cascading);
        addMergeOption(param, eMergeColor, cascading);
        addMergeOption(param, eMergeColorBurn, cascading);
        addMergeOption(param, eMergeColorDodge, cascading);
        addMergeOption(param, eMergeConjointOver, cascading);
        addMergeOption(param, eMergeCopy, cascading);
        addMergeOption(param, eMergeDifference, cascading);
        addMergeOption(param, eMergeDisjointOver, cascading);
        addMergeOption(param, eMergeDivide, cascading);
        addMergeOption(param, eMergeExclusion, cascading);
        addMergeOption(param, eMergeFreeze, cascading);
        addMergeOption(param, eMergeFrom, cascading);
        addMergeOption(param, eMergeGeometric, cascading);
        addMergeOption(param, eMergeGrainExtract, cascading);
        addMergeOption(param, eMergeGrainMerge, cascading);
        addMergeOption(param, eMergeHardLight, cascading);
        addMergeOption(param, eMergeHue, cascading);
        addMergeOption(param, eMergeHypot, cascading);
        addMergeOption(param, eMergeIn, cascading);
        //addMergeOption(param, eMergeInterpolated, cascading);
        addMergeOption(param, eMergeLuminosity, cascading);
        addMergeOption(param, eMergeMask, cascading);
        addMergeOption(param, eMergeMatte, cascading);
        addMergeOption(param, eMergeMax, cascading);
        addMergeOption(param, eMergeMin, cascading);
        addMergeOption(param, eMergeMinus, cascading);
        addMergeOption(param, eMergeMultiply, cascading);
        addMergeOption(param, eMergeOut, cascading);
        addMergeOption(param, eMergeOver, cascading);
        addMergeOption(param, eMergeOverlay, cascading);
        addMergeOption(param, eMergePinLight, cascading);
        addMergeOption(param, eMergePlus, cascading);
        addMergeOption(param, eMergeReflect, cascading);
        addMergeOption(param, eMergeSaturation, cascading);
        addMergeOption(param, eMergeScreen, cascading);
        addMergeOption(param, eMergeSoftLight, cascading);
        addMergeOption(param, eMergeStencil, cascading);
        addMergeOption(param, eMergeUnder, cascading);
        addMergeOption(param, eMergeXOR, cascading);
        param->setDefault( getDefaultOperation(plugin) );
        param->setAnimates(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }

    // boundingBox
    {
        ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamBBox);
        param->setLabel(kParamBBoxLabel);
        param->setHint(kParamBBoxHint);
        assert(param->getNOptions() == (int)eBBoxUnion);
        param->appendOption(kParamBBoxOptionUnion, kParamBBoxOptionUnionHint);
        assert(param->getNOptions() == (int)eBBoxIntersection);
        param->appendOption(kParamBBoxOptionIntersection, kParamBBoxOptionIntersectionHint);
        assert(param->getNOptions() == (int)eBBoxA);
        param->appendOption(kParamBBoxOptionA, kParamBBoxOptionAHint);
        assert(param->getNOptions() == (int)eBBoxB);
        param->appendOption(kParamBBoxOptionB, kParamBBoxOptionBHint);
        param->setAnimates(true);
        param->setDefault( (int)eBBoxUnion );
        if (page) {
            page->addChild(*param);
        }
    }

    // alphaMasking
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAlphaMasking);
        param->setLabel(kParamAlphaMaskingLabel);
        param->setAnimates(true);
        param->setDefault(false);
        param->setEnabled( MergeImages2D::isMaskable(eMergeOver) );
        param->setHint(kParamAlphaMaskingHint);
        param->setLayoutHint(eLayoutHintDivider);
        if (page) {
            page->addChild(*param);
        }
    }

#ifdef OFX_EXTENSIONS_NATRON
    desc.setChannelSelector(ePixelComponentNone); // we have our own channel selector
#endif
    {
        StringParamDescriptor* param = desc.defineStringParam(kParamAChannels);
        param->setLabel("");
        param->setHint(kParamAChannelsHint);
        param->setDefault(kParamAChannelsLabel);
        param->setStringType(eStringTypeLabel);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAChannelsR);
        param->setLabel(kParamAChannelsRLabel);
        param->setHint(kParamAChannelsRHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAChannelsG);
        param->setLabel(kParamAChannelsGLabel);
        param->setHint(kParamAChannelsGHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAChannelsB);
        param->setLabel(kParamAChannelsBLabel);
        param->setHint(kParamAChannelsBHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAChannelsA);
        param->setLabel(kParamAChannelsALabel);
        param->setHint(kParamAChannelsAHint);
        param->setDefault(true);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        StringParamDescriptor* param = desc.defineStringParam(kParamBChannels);
        param->setLabel("");
        param->setHint(kParamBChannelsHint);
        param->setDefault(kParamBChannelsLabel);
        param->setStringType(eStringTypeLabel);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamBChannelsR);
        param->setLabel(kParamBChannelsRLabel);
        param->setHint(kParamBChannelsRHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamBChannelsG);
        param->setLabel(kParamBChannelsGLabel);
        param->setHint(kParamBChannelsGHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamBChannelsB);
        param->setLabel(kParamBChannelsBLabel);
        param->setHint(kParamBChannelsBHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamBChannelsA);
        param->setLabel(kParamBChannelsALabel);
        param->setHint(kParamBChannelsAHint);
        param->setDefault(true);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        StringParamDescriptor* param = desc.defineStringParam(kParamOutputChannels);
        param->setLabel("");
        param->setHint(kParamOutputChannelsHint);
        param->setDefault(kParamOutputChannelsLabel);
        param->setStringType(eStringTypeLabel);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamOutputChannelsR);
        param->setLabel(kParamOutputChannelsRLabel);
        param->setHint(kParamOutputChannelsRHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamOutputChannelsG);
        param->setLabel(kParamOutputChannelsGLabel);
        param->setHint(kParamOutputChannelsGHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamOutputChannelsB);
        param->setLabel(kParamOutputChannelsBLabel);
        param->setHint(kParamOutputChannelsBHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        BooleanParamDescriptor* param = desc.defineBooleanParam(kParamOutputChannelsA);
        param->setLabel(kParamOutputChannelsALabel);
        param->setHint(kParamOutputChannelsAHint);
        param->setDefault(true);
        param->setLayoutHint(eLayoutHintDivider);
        if (page) {
            page->addChild(*param);
        }
    }

    if ( getImageEffectHostDescription()->supportsPixelComponent(ePixelComponentRGB) ) {
        // two hidden parameters to keep track of the fact that the user explicitely checked or unchecked tha "A" checkbox
        {
            BooleanParamDescriptor* param = desc.defineBooleanParam(kParamAChannelsAChanged);
            param->setDefault(false);
            param->setIsSecretAndDisabled(true);
            param->setAnimates(false);
            param->setEvaluateOnChange(false);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            BooleanParamDescriptor* param = desc.defineBooleanParam(kParamBChannelsAChanged);
            param->setDefault(false);
            param->setIsSecretAndDisabled(true);
            param->setAnimates(false);
            param->setEvaluateOnChange(false);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    ofxsMaskMixDescribeParams(desc, page);
} // >::describeInContext

template<MergePluginEnum plugin>
ImageEffect*
MergePluginFactory<plugin>::createInstance(OfxImageEffectHandle handle,
                                           ContextEnum /*context*/)
{
    assert(unsignedToString(12345) == "12345");
    //Natron >= 2.0 allows multiple inputs to be folded like the viewer node, so use this to merge
    //more than 2 images
    bool numerousInputs =  (plugin != eMergePluginRoto &&
                            getImageEffectHostDescription()->isNatron &&
                            getImageEffectHostDescription()->versionMajor >= 2);

    return new MergePlugin(handle, plugin, numerousInputs);
}


static MergePluginFactory<eMergePluginMerge>        p1(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginPlus>        p2(kPluginIdentifierSub "Plus", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginMatte>       p3(kPluginIdentifierSub "Matte", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginMultiply>    p4(kPluginIdentifierSub "Multiply", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginIn>          p5(kPluginIdentifierSub "In", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginOut>         p6(kPluginIdentifierSub "Out", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginScreen>      p7(kPluginIdentifierSub "Screen", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginMax>         p8(kPluginIdentifierSub "Max", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginMin>         p9(kPluginIdentifierSub "Min", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginAbsMinus>   p10(kPluginIdentifierSub "Difference", kPluginVersionMajor, kPluginVersionMinor);
static MergePluginFactory<eMergePluginRoto>       p11(kPluginIdentifierSub "Roto", kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p1)
mRegisterPluginFactoryInstance(p2)
mRegisterPluginFactoryInstance(p3)
mRegisterPluginFactoryInstance(p4)
mRegisterPluginFactoryInstance(p5)
mRegisterPluginFactoryInstance(p6)
mRegisterPluginFactoryInstance(p7)
mRegisterPluginFactoryInstance(p8)
mRegisterPluginFactoryInstance(p9)
mRegisterPluginFactoryInstance(p10)
mRegisterPluginFactoryInstance(p11)

OFXS_NAMESPACE_ANONYMOUS_EXIT
