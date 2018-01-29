
/*=========================================================================*\

    Copyright (c) Microsoft Corporation.  All rights reserved.

    File: D2D1helper.h

    Module Name: D2D

    Description: Helper files over the D2D interfaces and APIs.

\*=========================================================================*/
#pragma once

#ifndef _D2D1_HELPER_H_
#define _D2D1_HELPER_H_

#ifndef _D2D1_H_
#include <d2d1.h>
#endif // #ifndef _D2D1_H_

#ifndef D2D_USE_C_DEFINITIONS

namespace D2D1
{
    //
    // Forward declared IdentityMatrix function to allow matrix class to use
    // these constructors.
    // 
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_MATRIX_3X2_F
    IdentityMatrix();

    //
    // The default trait type for objects in D2D is float.
    //
    template<typename Type>
    struct TypeTraits
    {
        typedef D2D1_POINT_2F Point;
        typedef D2D1_SIZE_F   Size;
        typedef D2D1_RECT_F   Rect;
    };

    template<>
    struct TypeTraits<UINT32>
    {
        typedef D2D1_POINT_2U Point;
        typedef D2D1_SIZE_U   Size;
        typedef D2D1_RECT_U   Rect;
    };

    static
    COM_DECLSPEC_NOTHROW
    inline
    FLOAT FloatMax()
    {
        #ifdef FLT_MAX
            return FLT_MAX;
        #else
            return 3.402823466e+38F;
        #endif
    }

    //
    // Construction helpers
    //
    template<typename Type>
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    typename TypeTraits<Type>::Point
    Point2(
        Type x,
        Type y
        )
    {
        typename TypeTraits<Type>::Point point = { x, y };

        return point;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_POINT_2F
    Point2F(
        FLOAT x = 0.f,
        FLOAT y = 0.f
        )
    {
        return Point2<FLOAT>(x, y);
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_POINT_2U
    Point2U(
        UINT32 x = 0,
        UINT32 y = 0
        )
    {
        return Point2<UINT32>(x, y);
    }

    template<typename Type>
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    typename TypeTraits<Type>::Size
    Size(
        Type width,
        Type height
        )
    {
        typename TypeTraits<Type>::Size size = { width, height };

        return size;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_SIZE_F
    SizeF(
        FLOAT width = 0.f,
        FLOAT height = 0.f
        )
    {
        return Size<FLOAT>(width, height);
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_SIZE_U
    SizeU(
        UINT32 width = 0,
        UINT32 height = 0
        )
    {
        return Size<UINT32>(width, height);
    }

    template<typename Type>
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    typename TypeTraits<Type>::Rect
    Rect(
        Type left,
        Type top,
        Type right,
        Type bottom
        )
    {
        typename TypeTraits<Type>::Rect rect = { left, top, right, bottom };

        return rect;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_RECT_F
    RectF(
        FLOAT left = 0.f,
        FLOAT top = 0.f,
        FLOAT right = 0.f,
        FLOAT bottom = 0.f
        )
    {
        return Rect<FLOAT>(left, top, right, bottom);
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_RECT_U
    RectU(
        UINT32 left = 0,
        UINT32 top = 0,
        UINT32 right = 0,
        UINT32 bottom = 0
        )
    {
        return Rect<UINT32>(left, top, right, bottom);
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_RECT_F
    InfiniteRect()
    {
        D2D1_RECT_F rect = { -FloatMax(), -FloatMax(), FloatMax(),  FloatMax() };

        return rect;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_ARC_SEGMENT
    ArcSegment(
        _In_ CONST D2D1_POINT_2F &point,
        _In_ CONST D2D1_SIZE_F &size,
        _In_ FLOAT rotationAngle,
        _In_ D2D1_SWEEP_DIRECTION sweepDirection,
        _In_ D2D1_ARC_SIZE arcSize
        )
    {
        D2D1_ARC_SEGMENT arcSegment = { point, size, rotationAngle, sweepDirection, arcSize };

        return arcSegment;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_BEZIER_SEGMENT
    BezierSegment(
        _In_ CONST D2D1_POINT_2F &point1,
        _In_ CONST D2D1_POINT_2F &point2,
        _In_ CONST D2D1_POINT_2F &point3
        )
    {
        D2D1_BEZIER_SEGMENT bezierSegment = { point1, point2, point3 };

        return bezierSegment;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_ELLIPSE
    Ellipse(
        _In_ CONST D2D1_POINT_2F &center,
        FLOAT radiusX,
        FLOAT radiusY
        )
    {
        D2D1_ELLIPSE ellipse;

        ellipse.point = center;
        ellipse.radiusX = radiusX;
        ellipse.radiusY = radiusY;

        return ellipse;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_ROUNDED_RECT
    RoundedRect(
        _In_ CONST D2D1_RECT_F &rect,
        FLOAT radiusX,
        FLOAT radiusY
        )
    {
        D2D1_ROUNDED_RECT roundedRect;

        roundedRect.rect = rect;
        roundedRect.radiusX = radiusX;
        roundedRect.radiusY = radiusY;

        return roundedRect;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_BRUSH_PROPERTIES
    BrushProperties(
        _In_ FLOAT opacity = 1.0,
        _In_ CONST D2D1_MATRIX_3X2_F &transform = D2D1::IdentityMatrix()
        )
    {
        D2D1_BRUSH_PROPERTIES brushProperties;

        brushProperties.opacity = opacity;
        brushProperties.transform = transform;

        return brushProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_GRADIENT_STOP
    GradientStop(
        FLOAT position,
        _In_ CONST D2D1_COLOR_F &color
        )
    {
        D2D1_GRADIENT_STOP gradientStop = { position, color };

        return gradientStop;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_QUADRATIC_BEZIER_SEGMENT
    QuadraticBezierSegment(
        _In_ CONST D2D1_POINT_2F &point1,
        _In_ CONST D2D1_POINT_2F &point2
        )
    {
        D2D1_QUADRATIC_BEZIER_SEGMENT quadraticBezier = { point1, point2 };

        return quadraticBezier;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_STROKE_STYLE_PROPERTIES
    StrokeStyleProperties(
        D2D1_CAP_STYLE startCap = D2D1_CAP_STYLE_FLAT,
        D2D1_CAP_STYLE endCap = D2D1_CAP_STYLE_FLAT,
        D2D1_CAP_STYLE dashCap = D2D1_CAP_STYLE_FLAT,
        D2D1_LINE_JOIN lineJoin = D2D1_LINE_JOIN_MITER,
        FLOAT miterLimit = 10.0f,
        D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID,
        FLOAT dashOffset = 0.0f
        )
    {
        D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties;

        strokeStyleProperties.startCap = startCap;
        strokeStyleProperties.endCap = endCap;
        strokeStyleProperties.dashCap = dashCap;
        strokeStyleProperties.lineJoin = lineJoin;
        strokeStyleProperties.miterLimit = miterLimit;
        strokeStyleProperties.dashStyle = dashStyle;
        strokeStyleProperties.dashOffset = dashOffset;

        return strokeStyleProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_BITMAP_BRUSH_PROPERTIES
    BitmapBrushProperties(
        D2D1_EXTEND_MODE extendModeX = D2D1_EXTEND_MODE_CLAMP,
        D2D1_EXTEND_MODE extendModeY = D2D1_EXTEND_MODE_CLAMP,
        D2D1_BITMAP_INTERPOLATION_MODE interpolationMode = D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
        )
    {
        D2D1_BITMAP_BRUSH_PROPERTIES bitmapBrushProperties;

        bitmapBrushProperties.extendModeX = extendModeX;
        bitmapBrushProperties.extendModeY = extendModeY;
        bitmapBrushProperties.interpolationMode = interpolationMode;

        return bitmapBrushProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES
    LinearGradientBrushProperties(
        _In_ CONST D2D1_POINT_2F &startPoint,
        _In_ CONST D2D1_POINT_2F &endPoint
        )
    {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linearGradientBrushProperties;

        linearGradientBrushProperties.startPoint = startPoint;
        linearGradientBrushProperties.endPoint = endPoint;

        return linearGradientBrushProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES
    RadialGradientBrushProperties(
        _In_ CONST D2D1_POINT_2F &center,
        _In_ CONST D2D1_POINT_2F &gradientOriginOffset,
        FLOAT radiusX,
        FLOAT radiusY
        )
    {
        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radialGradientBrushProperties;

        radialGradientBrushProperties.center = center;
        radialGradientBrushProperties.gradientOriginOffset = gradientOriginOffset;
        radialGradientBrushProperties.radiusX = radiusX;
        radialGradientBrushProperties.radiusY = radiusY;

        return radialGradientBrushProperties;
    }

    //
    // PixelFormat
    // 
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_PIXEL_FORMAT
    PixelFormat(
        _In_ DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN,
        _In_ D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_UNKNOWN
        )
    {
        D2D1_PIXEL_FORMAT pixelFormat;

        pixelFormat.format = dxgiFormat;
        pixelFormat.alphaMode = alphaMode;

        return pixelFormat;
    }

    //
    // Bitmaps
    // 
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_BITMAP_PROPERTIES
    BitmapProperties(
        CONST D2D1_PIXEL_FORMAT &pixelFormat = D2D1::PixelFormat(),
        FLOAT dpiX = 96.0f,
        FLOAT dpiY = 96.0f
        )
    {
        D2D1_BITMAP_PROPERTIES bitmapProperties;

        bitmapProperties.pixelFormat = pixelFormat;
        bitmapProperties.dpiX = dpiX;
        bitmapProperties.dpiY = dpiY;

        return bitmapProperties;
    }

    //
    // Render Targets
    // 
    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_RENDER_TARGET_PROPERTIES
    RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE type =  D2D1_RENDER_TARGET_TYPE_DEFAULT,
        _In_ CONST D2D1_PIXEL_FORMAT &pixelFormat = D2D1::PixelFormat(),
        FLOAT dpiX = 0.0,
        FLOAT dpiY = 0.0,
        D2D1_RENDER_TARGET_USAGE usage = D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL  minLevel = D2D1_FEATURE_LEVEL_DEFAULT
        )
    {
        D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties;

        renderTargetProperties.type = type;
        renderTargetProperties.pixelFormat = pixelFormat;
        renderTargetProperties.dpiX = dpiX;
        renderTargetProperties.dpiY = dpiY;
        renderTargetProperties.usage = usage;
        renderTargetProperties.minLevel = minLevel;

        return renderTargetProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_HWND_RENDER_TARGET_PROPERTIES
    HwndRenderTargetProperties(
        _In_ HWND hwnd,
        _In_ D2D1_SIZE_U pixelSize = D2D1::Size(static_cast<UINT32>(0), static_cast<UINT32>(0)),
        _In_ D2D1_PRESENT_OPTIONS presentOptions = D2D1_PRESENT_OPTIONS_NONE
        )
    {
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProperties;

        hwndRenderTargetProperties.hwnd = hwnd;
        hwndRenderTargetProperties.pixelSize = pixelSize;
        hwndRenderTargetProperties.presentOptions = presentOptions;

        return hwndRenderTargetProperties;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_LAYER_PARAMETERS
    LayerParameters(
        _In_ CONST D2D1_RECT_F &contentBounds = D2D1::InfiniteRect(),
        _In_opt_ ID2D1Geometry *geometricMask = NULL,
        D2D1_ANTIALIAS_MODE maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
        D2D1_MATRIX_3X2_F maskTransform = D2D1::IdentityMatrix(),
        FLOAT opacity = 1.0,
        _In_opt_ ID2D1Brush *opacityBrush = NULL,
        D2D1_LAYER_OPTIONS layerOptions = D2D1_LAYER_OPTIONS_NONE
        )
    {
        D2D1_LAYER_PARAMETERS layerParameters = { 0 };

        layerParameters.contentBounds = contentBounds;
        layerParameters.geometricMask = geometricMask;
        layerParameters.maskAntialiasMode = maskAntialiasMode;
        layerParameters.maskTransform = maskTransform;
        layerParameters.opacity = opacity;
        layerParameters.opacityBrush = opacityBrush;
        layerParameters.layerOptions = layerOptions;

        return layerParameters;
    }

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_DRAWING_STATE_DESCRIPTION
    DrawingStateDescription(
        D2D1_ANTIALIAS_MODE antialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
        D2D1_TEXT_ANTIALIAS_MODE textAntialiasMode = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT,
        D2D1_TAG tag1 = 0,
        D2D1_TAG tag2 = 0,
        _In_ const D2D1_MATRIX_3X2_F &transform = D2D1::IdentityMatrix()
        )
    {
        D2D1_DRAWING_STATE_DESCRIPTION drawingStateDescription;

        drawingStateDescription.antialiasMode = antialiasMode;
        drawingStateDescription.textAntialiasMode = textAntialiasMode;
        drawingStateDescription.tag1 = tag1;
        drawingStateDescription.tag2 = tag2;
        drawingStateDescription.transform = transform;

        return drawingStateDescription;
    }

    //
    // Colors, this enum defines a set of predefined colors.
    //
    class ColorF : public D2D1_COLOR_F
    {
    public:

        enum Enum
        {
            AliceBlue = 0xF0F8FF,
            AntiqueWhite = 0xFAEBD7,
            Aqua = 0x00FFFF,
            Aquamarine = 0x7FFFD4,
            Azure = 0xF0FFFF,
            Beige = 0xF5F5DC,
            Bisque = 0xFFE4C4,
            Black = 0x000000,
            BlanchedAlmond = 0xFFEBCD,
            Blue = 0x0000FF,
            BlueViolet = 0x8A2BE2,
            Brown = 0xA52A2A,
            BurlyWood = 0xDEB887,
            CadetBlue = 0x5F9EA0,
            Chartreuse = 0x7FFF00,
            Chocolate = 0xD2691E,
            Coral = 0xFF7F50,
            CornflowerBlue = 0x6495ED,
            Cornsilk = 0xFFF8DC,
            Crimson = 0xDC143C,
            Cyan = 0x00FFFF,
            DarkBlue = 0x00008B,
            DarkCyan = 0x008B8B,
            DarkGoldenrod = 0xB8860B,
            DarkGray = 0xA9A9A9,
            DarkGreen = 0x006400,
            DarkKhaki = 0xBDB76B,
            DarkMagenta = 0x8B008B,
            DarkOliveGreen = 0x556B2F,
            DarkOrange = 0xFF8C00,
            DarkOrchid = 0x9932CC,
            DarkRed = 0x8B0000,
            DarkSalmon = 0xE9967A,
            DarkSeaGreen = 0x8FBC8F,
            DarkSlateBlue = 0x483D8B,
            DarkSlateGray = 0x2F4F4F,
            DarkTurquoise = 0x00CED1,
            DarkViolet = 0x9400D3,
            DeepPink = 0xFF1493,
            DeepSkyBlue = 0x00BFFF,
            DimGray = 0x696969,
            DodgerBlue = 0x1E90FF,
            Firebrick = 0xB22222,
            FloralWhite = 0xFFFAF0,
            ForestGreen = 0x228B22,
            Fuchsia = 0xFF00FF,
            Gainsboro = 0xDCDCDC,
            GhostWhite = 0xF8F8FF,
            Gold = 0xFFD700,
            Goldenrod = 0xDAA520,
            Gray = 0x808080,
            Green = 0x008000,
            GreenYellow = 0xADFF2F,
            Honeydew = 0xF0FFF0,
            HotPink = 0xFF69B4,
            IndianRed = 0xCD5C5C,
            Indigo = 0x4B0082,
            Ivory = 0xFFFFF0,
            Khaki = 0xF0E68C,
            Lavender = 0xE6E6FA,
            LavenderBlush = 0xFFF0F5,
            LawnGreen = 0x7CFC00,
            LemonChiffon = 0xFFFACD,
            LightBlue = 0xADD8E6,
            LightCoral = 0xF08080,
            LightCyan = 0xE0FFFF,
            LightGoldenrodYellow = 0xFAFAD2,
            LightGreen = 0x90EE90,
            LightGray = 0xD3D3D3,
            LightPink = 0xFFB6C1,
            LightSalmon = 0xFFA07A,
            LightSeaGreen = 0x20B2AA,
            LightSkyBlue = 0x87CEFA,
            LightSlateGray = 0x778899,
            LightSteelBlue = 0xB0C4DE,
            LightYellow = 0xFFFFE0,
            Lime = 0x00FF00,
            LimeGreen = 0x32CD32,
            Linen = 0xFAF0E6,
            Magenta = 0xFF00FF,
            Maroon = 0x800000,
            MediumAquamarine = 0x66CDAA,
            MediumBlue = 0x0000CD,
            MediumOrchid = 0xBA55D3,
            MediumPurple = 0x9370DB,
            MediumSeaGreen = 0x3CB371,
            MediumSlateBlue = 0x7B68EE,
            MediumSpringGreen = 0x00FA9A,
            MediumTurquoise = 0x48D1CC,
            MediumVioletRed = 0xC71585,
            MidnightBlue = 0x191970,
            MintCream = 0xF5FFFA,
            MistyRose = 0xFFE4E1,
            Moccasin = 0xFFE4B5,
            NavajoWhite = 0xFFDEAD,
            Navy = 0x000080,
            OldLace = 0xFDF5E6,
            Olive = 0x808000,
            OliveDrab = 0x6B8E23,
            Orange = 0xFFA500,
            OrangeRed = 0xFF4500,
            Orchid = 0xDA70D6,
            PaleGoldenrod = 0xEEE8AA,
            PaleGreen = 0x98FB98,
            PaleTurquoise = 0xAFEEEE,
            PaleVioletRed = 0xDB7093,
            PapayaWhip = 0xFFEFD5,
            PeachPuff = 0xFFDAB9,
            Peru = 0xCD853F,
            Pink = 0xFFC0CB,
            Plum = 0xDDA0DD,
            PowderBlue = 0xB0E0E6,
            Purple = 0x800080,
            Red = 0xFF0000,
            RosyBrown = 0xBC8F8F,
            RoyalBlue = 0x4169E1,
            SaddleBrown = 0x8B4513,
            Salmon = 0xFA8072,
            SandyBrown = 0xF4A460,
            SeaGreen = 0x2E8B57,
            SeaShell = 0xFFF5EE,
            Sienna = 0xA0522D,
            Silver = 0xC0C0C0,
            SkyBlue = 0x87CEEB,
            SlateBlue = 0x6A5ACD,
            SlateGray = 0x708090,
            Snow = 0xFFFAFA,
            SpringGreen = 0x00FF7F,
            SteelBlue = 0x4682B4,
            Tan = 0xD2B48C,
            Teal = 0x008080,
            Thistle = 0xD8BFD8,
            Tomato = 0xFF6347,
            Turquoise = 0x40E0D0,
            Violet = 0xEE82EE,
            Wheat = 0xF5DEB3,
            White = 0xFFFFFF,
            WhiteSmoke = 0xF5F5F5,
            Yellow = 0xFFFF00,
            YellowGreen = 0x9ACD32,
        };

        //
        // Construct a color, note that the alpha value from the "rgb" component
        // is never used.
        // 
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        ColorF(
            UINT32 rgb,
            FLOAT a = 1.0
            )
        {
            Init(rgb, a);
        }

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        ColorF(
            Enum knownColor,
            FLOAT a = 1.0
            )
        {
            Init(knownColor, a);
        }

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        ColorF(
            FLOAT red,
            FLOAT green,
            FLOAT blue,
            FLOAT alpha = 1.0
            )
        {
            r = red;
            g = green;
            b = blue;
            a = alpha;
        }

    private:

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        void
        Init(
            UINT32 rgb,
            FLOAT alpha
            )
        {
            r = static_cast<FLOAT>((rgb & sc_redMask) >> sc_redShift) / 255.f;
            g = static_cast<FLOAT>((rgb & sc_greenMask) >> sc_greenShift) / 255.f;
            b = static_cast<FLOAT>((rgb & sc_blueMask) >> sc_blueShift) / 255.f;
            a = alpha;
        }

        static const UINT32 sc_redShift   = 16;
        static const UINT32 sc_greenShift = 8;
        static const UINT32 sc_blueShift  = 0;

        static const UINT32 sc_redMask = 0xff << sc_redShift;
        static const UINT32 sc_greenMask = 0xff << sc_greenShift;
        static const UINT32 sc_blueMask = 0xff << sc_blueShift;
    };

    class Matrix3x2F : public D2D1_MATRIX_3X2_F
    {
    public:

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F(
            FLOAT m11,
            FLOAT m12,
            FLOAT m21,
            FLOAT m22,
            FLOAT m31,
            FLOAT m32
            )
        {
            _11 = m11;
            _12 = m12;
            _21 = m21;
            _22 = m22;
            _31 = m31;
            _32 = m32;
        }

        //
        // Creates an uninitialized matrix
        // 
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F(
            )
        {
        }

        //
        // Named quasi-constructors
        //
        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Identity()
        {
            Matrix3x2F identity;

            identity._11 = 1.f;
            identity._12 = 0.f;
            identity._21 = 0.f;
            identity._22 = 1.f;
            identity._31 = 0.f;
            identity._32 = 0.f;

            return identity;
        }

        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Translation(
            D2D1_SIZE_F size
            )
        {
            Matrix3x2F translation;

            translation._11 = 1.0; translation._12 = 0.0;
            translation._21 = 0.0; translation._22 = 1.0;
            translation._31 = size.width; translation._32 = size.height;

            return translation;
        }

        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Translation(
            FLOAT x,
            FLOAT y
            )
        {
            return Translation(SizeF(x, y));
        }


        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Scale(
            D2D1_SIZE_F size,
            D2D1_POINT_2F center = D2D1::Point2F()
            )
        {
            Matrix3x2F scale;

            scale._11 = size.width; scale._12 = 0.0;
            scale._21 = 0.0; scale._22 = size.height;
            scale._31 = center.x - size.width * center.x;
            scale._32 = center.y - size.height * center.y;

            return scale;
        }

        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Scale(
            FLOAT x,
            FLOAT y,
            D2D1_POINT_2F center = D2D1::Point2F()
            )
        {
            return Scale(SizeF(x, y), center);
        }

        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Rotation(
            FLOAT angle,
            D2D1_POINT_2F center = D2D1::Point2F()
            )
        {
            Matrix3x2F rotation;

            D2D1MakeRotateMatrix(angle, center, &rotation);

            return rotation;
        }

        static
        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        Skew(
            FLOAT angleX,
            FLOAT angleY,
            D2D1_POINT_2F center = D2D1::Point2F()
            )
        {
            Matrix3x2F skew;

            D2D1MakeSkewMatrix(angleX, angleY, center, &skew);

            return skew;
        }

        //
        // Functions for convertion from the base D2D1_MATRIX_3X2_F to this type
        // without making a copy
        //
        static
        COM_DECLSPEC_NOTHROW
        inline
        const Matrix3x2F*
        ReinterpretBaseType(const D2D1_MATRIX_3X2_F *pMatrix)
        {
            return static_cast<const Matrix3x2F *>(pMatrix);
        }

        static
        COM_DECLSPEC_NOTHROW
        inline
        Matrix3x2F*
        ReinterpretBaseType(D2D1_MATRIX_3X2_F *pMatrix)
        {
            return static_cast<Matrix3x2F *>(pMatrix);
        }

        COM_DECLSPEC_NOTHROW
        inline
        FLOAT
        Determinant() const
        {
            return (_11 * _22) - (_12 * _21);
        }

        COM_DECLSPEC_NOTHROW
        inline
        bool
        IsInvertible() const
        {
            return !!D2D1IsMatrixInvertible(this);
        }

        COM_DECLSPEC_NOTHROW
        inline
        bool
        Invert()
        {
            return !!D2D1InvertMatrix(this);
        }

        COM_DECLSPEC_NOTHROW
        inline
        bool
        IsIdentity() const
        {
            return     _11 == 1.f && _12 == 0.f
                    && _21 == 0.f && _22 == 1.f
                    && _31 == 0.f && _32 == 0.f;
        }

        COM_DECLSPEC_NOTHROW
        inline
        void SetProduct(
            const Matrix3x2F &a,
            const Matrix3x2F &b
            )
        {
            _11 = a._11 * b._11 + a._12 * b._21;
            _12 = a._11 * b._12 + a._12 * b._22;
            _21 = a._21 * b._11 + a._22 * b._21;
            _22 = a._21 * b._12 + a._22 * b._22;
            _31 = a._31 * b._11 + a._32 * b._21 + b._31;
            _32 = a._31 * b._12 + a._32 * b._22 + b._32;
        }

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        Matrix3x2F
        operator*(
            const Matrix3x2F &matrix
            ) const
        {
            Matrix3x2F result;

            result.SetProduct(*this, matrix);

            return result;
        }

        COM_DECLSPEC_NOTHROW
        D2D1FORCEINLINE
        D2D1_POINT_2F
        TransformPoint(
            D2D1_POINT_2F point
            ) const
        {
            D2D1_POINT_2F result =
            {
                point.x * _11 + point.y * _21 + _31,
                point.x * _12 + point.y * _22 + _32
            };

            return result;
        }
    };

    COM_DECLSPEC_NOTHROW
    D2D1FORCEINLINE
    D2D1_POINT_2F
    operator*(
        const D2D1_POINT_2F &point,
        const D2D1_MATRIX_3X2_F &matrix
        )
    {
        return Matrix3x2F::ReinterpretBaseType(&matrix)->TransformPoint(point);
    }

    COM_DECLSPEC_NOTHROW
    D2D1_MATRIX_3X2_F
    IdentityMatrix()
    {
        return Matrix3x2F::Identity();
    }

} // namespace D2D1

COM_DECLSPEC_NOTHROW
D2D1FORCEINLINE
D2D1_MATRIX_3X2_F
operator*(
    const D2D1_MATRIX_3X2_F &matrix1,
    const D2D1_MATRIX_3X2_F &matrix2
    )
{
    return
        (*D2D1::Matrix3x2F::ReinterpretBaseType(&matrix1)) *
        (*D2D1::Matrix3x2F::ReinterpretBaseType(&matrix2));
}

COM_DECLSPEC_NOTHROW
inline
bool
operator==(const D2D1_SIZE_U &size1, const D2D1_SIZE_U &size2)
{
    return (size1.width == size2.width) && (size1.height == size2.height);
}

COM_DECLSPEC_NOTHROW
inline
bool
operator==(const D2D1_RECT_U &rect1, const D2D1_RECT_U &rect2)
{
    return (rect1.left  == rect2.left)  && (rect1.top     == rect2.top) &&
           (rect1.right == rect2.right) && (rect1.bottom  == rect2.bottom);
}

#endif // #ifndef D2D_USE_C_DEFINITIONS

#endif // #ifndef _D2D1_HELPER_H_

