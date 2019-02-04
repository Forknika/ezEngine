#include <PCH.h>

#include <Core/Assets/AssetFileHeader.h>
#include <Texture/Image/Formats/DdsFileFormat.h>
#include <Texture/TexConv/TexConvProcessor.h>
#include <Texture/ezTexFormat/ezTexFormat.h>

ezResult ezTexConvProcessor::Assemble2DTexture()
{
  m_pCurrentScratchImage->ResetAndAlloc(m_Descriptor.m_InputImages[0].GetHeader());

  ezColor* pPixelOut = m_pCurrentScratchImage->GetPixelPointer<ezColor>();

  return Assemble2DSlice(m_Descriptor.m_ChannelMappings[0], pPixelOut);
}

ezResult ezTexConvProcessor::Assemble2DSlice(const ezTexConvSliceChannelMapping& mapping, ezColor* pPixelOut)
{
  ezHybridArray<const ezColor*, 16> pSource;
  for (ezUInt32 i = 0; i < m_Descriptor.m_InputImages.GetCount(); ++i)
  {
    pSource.ExpandAndGetRef() = m_Descriptor.m_InputImages[i].GetPixelPointer<ezColor>();
  }

  const bool bFlip = m_Descriptor.m_bFlipHorizontal;

  for (ezUInt32 y = 0; y < m_uiTargetResolutionY; ++y)
  {
    const ezUInt32 pixelReadRowOffset = m_uiTargetResolutionX * y;
    const ezUInt32 pixelWriteRowOffset = m_uiTargetResolutionX * (bFlip ? (m_uiTargetResolutionY - y - 1) : y);

    for (ezUInt32 x = 0; x < m_uiTargetResolutionX; ++x)
    {

      for (ezUInt32 channel = 0; channel < 4; ++channel)
      {
        float fValue;

        const auto& cm = mapping.m_Channel[channel];
        const ezInt32 inputIndex = cm.m_iInputImageIndex;
        if (inputIndex != -1)
        {
          const ezColor* pSourcePixel = pSource[inputIndex] + pixelReadRowOffset + x;

          switch (cm.m_ChannelValue)
          {
            case ezTexConvChannelValue::Red:
              fValue = pSourcePixel->r;
              break;
            case ezTexConvChannelValue::Green:
              fValue = pSourcePixel->g;
              break;
            case ezTexConvChannelValue::Blue:
              fValue = pSourcePixel->b;
              break;
            case ezTexConvChannelValue::Alpha:
              fValue = pSourcePixel->a;
              break;

            default:
              EZ_ASSERT_NOT_IMPLEMENTED;
              break;
          }
        }
        else
        {
          switch (cm.m_ChannelValue)
          {
            case ezTexConvChannelValue::Black:
              fValue = 0.0f;
              break;

            default:
              fValue = 1.0f;
              break;
          }
        }

        pPixelOut[pixelWriteRowOffset + x].GetData()[channel] = fValue;
      }
    }
  }

  return EZ_SUCCESS;
}

ezResult ezTexConvProcessor::WriteTexFile(ezStreamWriter& stream, const ezImage& image)
{
  ezAssetFileHeader asset;
  asset.SetFileHashAndVersion(m_Descriptor.m_uiAssetHash, m_Descriptor.m_uiAssetVersion);

  asset.Write(stream);

  ezTexFormat texFormat;
  texFormat.m_bSRGB = ezImageFormat::IsSrgb(image.GetImageFormat());
  texFormat.m_WrapModeU = m_Descriptor.m_WrapModes[0];
  texFormat.m_WrapModeV = m_Descriptor.m_WrapModes[1];
  texFormat.m_WrapModeW = m_Descriptor.m_WrapModes[2];
  texFormat.m_TextureFilter = m_Descriptor.m_FilterMode;

  texFormat.WriteTextureHeader(stream);

  ezDdsFileFormat ddsWriter;
  if (ddsWriter.WriteImage(stream, image, ezLog::GetThreadLocalLogSystem(), "dds").Failed())
  {
    ezLog::Error("Failed to write DDS image chunk to ezTex file.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

EZ_STATICLINK_FILE(Texture, Texture_TexConv_Implementation_Texture2D);