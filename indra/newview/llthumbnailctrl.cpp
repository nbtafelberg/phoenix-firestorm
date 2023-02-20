/** 
 * @file llthumbnailctrl.cpp
 * @brief LLThumbnailCtrl base class
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llthumbnailctrl.h"

#include "linden_common.h"
#include "llagent.h"
#include "lluictrlfactory.h"
#include "lluuid.h"
#include "lltrans.h"
#include "llviewborder.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llwindow.h"

static LLDefaultChildRegistry::Register<LLThumbnailCtrl> r("thumbnail");

LLThumbnailCtrl::Params::Params()
: border("border")
, border_color("border_color")
, image_name("image_name")
, border_visible("show_visible", false)
, interactable("interactable", false)
, show_loading("show_loading", true)
{}

LLThumbnailCtrl::LLThumbnailCtrl(const LLThumbnailCtrl::Params& p)
:	LLUICtrl(p)
,   mBorderColor(p.border_color())
,   mBorderVisible(p.border_visible())
,   mInteractable(p.interactable())
,   mShowLoadingPlaceholder(p.show_loading())
,	mPriority(LLGLTexture::BOOST_PREVIEW)
{
    mLoadingPlaceholderString = LLTrans::getString("texture_loading");
    
    LLRect border_rect = getLocalRect();
    LLViewBorder::Params vbparams(p.border);
    vbparams.name("border");
    vbparams.rect(border_rect);
    mBorder = LLUICtrlFactory::create<LLViewBorder> (vbparams);
    addChild(mBorder);
    
    if (p.image_name.isProvided())
    {
        setValue(p.image_name());
    }
}

LLThumbnailCtrl::~LLThumbnailCtrl()
{
	mTexturep = nullptr;
    mImagep = nullptr;
}


void LLThumbnailCtrl::draw()
{
    LLRect draw_rect = getLocalRect();
    
    if (mBorderVisible)
    {
        mBorder->setKeyboardFocusHighlight(hasFocus());
        
        gl_rect_2d( draw_rect, mBorderColor.get(), FALSE );
        draw_rect.stretch( -1 );
    }

    // If we're in a focused floater, don't apply the floater's alpha to the texture.
    const F32 alpha = getTransparencyType() == TT_ACTIVE ? 1.0f : getCurrentTransparency();
    if( mTexturep )
    {
        if( mTexturep->getComponents() == 4 )
        {
            gl_rect_2d_checkerboard( draw_rect, alpha );
        }
        
        gl_draw_scaled_image( draw_rect.mLeft, draw_rect.mBottom, draw_rect.getWidth(), draw_rect.getHeight(), mTexturep, UI_VERTEX_COLOR % alpha);
        
        mTexturep->setKnownDrawSize(draw_rect.getWidth(), draw_rect.getHeight());
    }
    else if( mImagep.notNull() )
    {
        mImagep->draw(getLocalRect(), UI_VERTEX_COLOR % alpha );
    }
    else
    {
        gl_rect_2d( draw_rect, LLColor4::grey % alpha, TRUE );

        // Draw X
        gl_draw_x( draw_rect, LLColor4::black );
    }

    // Show "Loading..." string on the top left corner while this texture is loading.
    // Using the discard level, do not show the string if the texture is almost but not
    // fully loaded.
    if (mTexturep.notNull()
        && mShowLoadingPlaceholder
        && !mTexturep->isFullyLoaded())
    {
        U32 v_offset = 25;
        LLFontGL* font = LLFontGL::getFontSansSerif();

        // Don't show as loaded if the texture is almost fully loaded (i.e. discard1) unless god
        if ((mTexturep->getDiscardLevel() > 1) || gAgent.isGodlike())
        {
            font->renderUTF8(
                mLoadingPlaceholderString,
                0,
                llfloor(draw_rect.mLeft+3),
                llfloor(draw_rect.mTop-v_offset),
                LLColor4::white,
                LLFontGL::LEFT,
                LLFontGL::BASELINE,
                LLFontGL::DROP_SHADOW);
        }
    }

    LLUICtrl::draw();
}

// virtual
// value might be a string or a UUID
void LLThumbnailCtrl::setValue(const LLSD& value)
{
	LLSD tvalue(value);
	if (value.isString() && LLUUID::validate(value.asString()))
	{
		//RN: support UUIDs masquerading as strings
		tvalue = LLSD(LLUUID(value.asString()));
	}
    
	LLUICtrl::setValue(tvalue);
    
    mImageAssetID = LLUUID::null;
    mTexturep = nullptr;
    mImagep = nullptr;
    
	if (tvalue.isUUID())
	{
        mImageAssetID = tvalue.asUUID();
        if (mImageAssetID.notNull())
        {
            // Should it support baked textures?
            mTexturep = LLViewerTextureManager::getFetchedTexture(mImageAssetID, FTT_DEFAULT, MIPMAP_YES, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
            
            mTexturep->setBoostLevel(mPriority);
            mTexturep->forceToSaveRawImage(0);
            
            S32 desired_draw_width = mTexturep->getWidth();
            S32 desired_draw_height = mTexturep->getHeight();
            
            mTexturep->setKnownDrawSize(desired_draw_width, desired_draw_height);
        }
	}
    else if (tvalue.isString())
    {
        mImagep = LLUI::getUIImage(tvalue.asString(), LLGLTexture::BOOST_UI);
        if (mImagep)
        {
            LLViewerFetchedTexture* texture = dynamic_cast<LLViewerFetchedTexture*>(mImagep->getImage().get());
            if(texture)
            {
                mImageAssetID = texture->getID();
            }
        }
    }
}

BOOL LLThumbnailCtrl::handleHover(S32 x, S32 y, MASK mask)
{
    if (mInteractable && getEnabled())
    {
        getWindow()->setCursor(UI_CURSOR_HAND);
        return TRUE;
    }
    return LLUICtrl::handleHover(x, y, mask);
}

