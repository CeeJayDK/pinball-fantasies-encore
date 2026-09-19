#include "platform/ImageFile.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

namespace pfr {

std::optional<RgbaImage> loadImageFile(const std::filesystem::path& path) {
  const std::string s = path.string();
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(s.data()),
                                                         static_cast<CFIndex>(s.size()), false);
  if (!url) return std::nullopt;
  CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
  CFRelease(url);
  if (!source) return std::nullopt;
  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
  CFRelease(source);
  if (!image) return std::nullopt;

  RgbaImage out;
  out.width = static_cast<int>(CGImageGetWidth(image));
  out.height = static_cast<int>(CGImageGetHeight(image));
  out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4);
  // Drawing into a premultiplied sRGB bitmap leaves transparent areas black, which is what
  // the screens around the pictures are.
  CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGContextRef context = CGBitmapContextCreate(out.pixels.data(), static_cast<size_t>(out.width),
                                               static_cast<size_t>(out.height), 8,
                                               static_cast<size_t>(out.width) * 4, space,
                                               static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | kCGBitmapByteOrder32Big);
  CGColorSpaceRelease(space);
  if (!context) {
    CGImageRelease(image);
    return std::nullopt;
  }
  CGContextDrawImage(context, CGRectMake(0, 0, out.width, out.height), image);
  CGContextRelease(context);
  CGImageRelease(image);
  return out;
}

}  // namespace pfr
