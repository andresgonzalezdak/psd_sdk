#include "../PsdPch.h"
#include "PsdTextLayerUtils.h"

#include "../PsdMemoryUtil.h"
#include "../PsdSyncFileUtil.h"
#include "../PsdLayer.h"
#include "../PsdLayerType.h"

#include <vector>
#include <sstream>
#include <iostream>
#include <utility>
#include <regex>
#include <fstream>
#include <locale>
#include <codecvt>


PSD_NAMESPACE_BEGIN

namespace textLayers
{
	uint32_t GetPropertyFromLength(std::string& property, uint32_t length, SyncFileReader& reader, bool skip)
	{
		if (length == 0u)
		{
			if (skip)
			{
				reader.Skip(4u);
			}
			else
			{
				const uint32_t key = fileUtil::ReadFromFileBE<uint32_t>(reader);

				property = "";
				property.push_back(static_cast<char>((key >> 24) & 0xFF));
				property.push_back(static_cast<char>((key >> 16) & 0xFF));
				property.push_back(static_cast<char>((key >> 8) & 0xFF));
				property.push_back(static_cast<char>(key & 0xFF));
			}

			return 4u;
		}
		else
		{
			if (skip)
			{
				reader.Skip(length);
			}
			else
			{
				uint8_t* text = new uint8_t[length];
				reader.Read(text, length);

				property = std::string(reinterpret_cast<const char*>(text), length);

				delete[] text;
			}

			return length;
		}
	}

	uint32_t GetUnicodeString(std::wstring& readableString, SyncFileReader& reader, bool skip)
	{
		const uint32_t characterCountWithoutNull = fileUtil::ReadFromFileBE<uint32_t>(reader);

		if (skip)
		{
			reader.Skip(characterCountWithoutNull * sizeof(uint16_t));
		}
		else
		{
			uint16_t* unicodeText = new uint16_t[characterCountWithoutNull];
			reader.Read((void*)unicodeText, characterCountWithoutNull * sizeof(uint16_t));

			// convert to big endian
			for (uint32_t c = 0u; c < characterCountWithoutNull; ++c)
			{
				const uint16_t unicodeChar = unicodeText[c];
				unicodeText[c] = endianUtil::NativeToBigEndian(unicodeChar);
			}

			readableString = std::wstring(reinterpret_cast<const wchar_t*>(unicodeText), characterCountWithoutNull);

			delete[] unicodeText;
		}

		return characterCountWithoutNull * sizeof(uint16_t) + 4u;
	}

	uint32_t ParseProperty(SyncFileReader& reader, std::unordered_map<std::string, TraversalTreeNode>& propertyTree, bool skip)
	{
		const uint32_t length = fileUtil::ReadFromFileBE<uint32_t>(reader);

		std::string property;
		uint32_t byteCount = GetPropertyFromLength(property, length, reader) + 4u;

		const uint32_t key = fileUtil::ReadFromFileBE<uint32_t>(reader);
		byteCount += 4u;

		TraversalTreeNode newParentNode;

		auto iter = propertyTree.find(property);
		bool isTargetProperty = false;

		if (!skip)
		{
			isTargetProperty = iter != propertyTree.end();

			if (isTargetProperty)
			{
				isTargetProperty = true;
				newParentNode = iter->second;
			}
		}

		/*		char code[4];
				code[3] = static_cast<char>(key & 0xFF);
				code[2] = static_cast<char>((key >> 8) & 0xFF);
				code[1] = static_cast<char>((key >> 16) & 0xFF);
				code[0] = static_cast<char>((key >> 24) & 0xFF);
		*/

		if (key == util::Key<'o', 'b', 'j', ' '>::VALUE)
		{
			const uint32_t numReferences = fileUtil::ReadFromFileBE<uint32_t>(reader);

			for (uint32_t i = 0u; i < numReferences; ++i)
			{
				byteCount += ParseProperty(reader, newParentNode.children, !isTargetProperty);
			}

			byteCount += 4u;
		}
		else if (key == util::Key<'p', 'r', 'o', 'p'>::VALUE)
		{
			std::wstring noStringUnicode;
			std::string noString;
			byteCount += GetUnicodeString(noStringUnicode, reader, !isTargetProperty);
			uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);
			newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty) + 8u;

			// TODO (Andres): add callback
		}
		else if (key == util::Key<'E', 'n', 'm', 'r'>::VALUE)
		{
			std::wstring noStringUnicode;
			std::string noString;
			byteCount += GetUnicodeString(noStringUnicode, reader, !isTargetProperty);
			uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);
			newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);
			newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);
			byteCount += 12u;
		}
		else if (key == util::Key<'r', 'e', 'l', 'e'>::VALUE)
		{
			std::wstring noStringUnicode;
			std::string noString;
			byteCount += GetUnicodeString(noStringUnicode, reader, !isTargetProperty);
			const uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);

			if (isTargetProperty)
			{
				fileUtil::ReadFromFileBE<uint32_t>(reader);
			}
			else
			{
				reader.Skip(8u);
			}

			byteCount += 8u;
		}
		else if ((key == util::Key<'O', 'b', 'j', 'c'>::VALUE) ||
			(key == util::Key<'G', 'l', 'b', 'O'>::VALUE))
		{
			byteCount += ParseObjectDescriptor(reader, newParentNode, !isTargetProperty);
		}
		else if (key == util::Key<'V', 'l', 'L', 's'>::VALUE)
		{
			const uint32_t listLength = fileUtil::ReadFromFileBE<uint32_t>(reader);

			for (uint32_t i = 0u; i < listLength; ++i)
			{
				byteCount += ParseProperty(reader, newParentNode.children, !isTargetProperty);
			}

			byteCount += 4u;
		}
		else if (key == util::Key<'d', 'o', 'u', 'b'>::VALUE)
		{
			if (isTargetProperty)
			{
				float64_t value = fileUtil::ReadFromFileBE<float64_t>(reader);
				newParentNode.longCallback(value);
			}
			else
			{
				reader.Skip(8u);
			}

			byteCount += 8u;
		}
		else if (key == util::Key<'U', 'n', 't', 'F'>::VALUE)
		{
			if (isTargetProperty)
			{
				fileUtil::ReadFromFileBE<uint32_t>(reader);
				fileUtil::ReadFromFileBE<float64_t>(reader);

				// TODO (Andres): add callback for this
			}
			else
			{
				reader.Skip(12u);
			}

			byteCount += 12u;
		}
		else if (key == util::Key<'T', 'E', 'X', 'T'>::VALUE)
		{
			std::wstring noString;
			byteCount += GetUnicodeString(noString, reader, !isTargetProperty);

			if (isTargetProperty)
			{
				newParentNode.textUtf16Callback(noString);
			}
		}
		else if (key == util::Key<'e', 'n', 'u', 'm'>::VALUE)
		{
			std::string noString;
			uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty);
			newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty) + 8u;

			// TODO (Andres): add callback
		}
		else if (key == util::Key<'l', 'o', 'n', 'g'>::VALUE)
		{
			if (isTargetProperty)
			{
				uint32_t value = fileUtil::ReadFromFileBE<uint32_t>(reader);
				newParentNode.longCallback(value);
			}
			else
			{
				reader.Skip(4u);
			}

			byteCount += 4u;
		}
		else if (key == util::Key<'c', 'o', 'm', 'p'>::VALUE)
		{
			if (isTargetProperty)
			{
				uint64_t value = fileUtil::ReadFromFileBE<uint64_t>(reader);
				newParentNode.longCallback(value);
			}
			else
			{
				reader.Skip(8u);
			}

			byteCount += 8u;
		}
		else if (key == util::Key<'b', 'o', 'o', 'l'>::VALUE)
		{
			if (isTargetProperty)
			{
				uint8_t value = fileUtil::ReadFromFileBE<uint8_t>(reader);
				newParentNode.longCallback(value);
			}
			else
			{
				reader.Skip(1u);
			}

			byteCount++;
		}
		else if (key == util::Key<'C', 'l', 's', 's'>::VALUE ||
			key == util::Key<'t', 'y', 'p', 'e'>::VALUE ||
			key == util::Key<'G', 'l', 'b', 'C'>::VALUE)
		{
			std::wstring noStringUnicode;
			std::string noString;
			byteCount += GetUnicodeString(noStringUnicode, reader, !isTargetProperty);
			const uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			byteCount += GetPropertyFromLength(noString, newLength, reader, !isTargetProperty) + 4u;

			// TODO (Andres): add callback
		}
		else if (key == util::Key<'a', 'l', 'i', 's'>::VALUE)
		{
			const uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);
			reader.Skip(newLength);
			byteCount += 4u + newLength;
		}
		else if (key == util::Key<'t', 'd', 't', 'a'>::VALUE)
		{
			uint32_t newLength = fileUtil::ReadFromFileBE<uint32_t>(reader);

			if (isTargetProperty)
			{
				uint8_t* rawData = new uint8_t[newLength];
				reader.Read(rawData, newLength);

				std::string rawDataString(reinterpret_cast<const char*>(rawData), newLength);
				newParentNode.textCallback(rawDataString);

				delete[] rawData;
			}
			else
			{
				reader.Skip(newLength);
			}

			byteCount += 4u + newLength;
		}
		else
		{
			if (isTargetProperty)
			{
				uint64_t value = fileUtil::ReadFromFileBE<uint64_t>(reader);
				newParentNode.longCallback(value);
			}
			else
			{
				reader.Skip(8u);
			}

			byteCount += 8u;
		}

		return byteCount;
	}

	uint32_t ParseObjectDescriptor(SyncFileReader& reader, TraversalTreeNode& node, bool skip)
	{
		std::wstring noStringUnicode;
		std::string noString;
		uint32_t byteCount = GetUnicodeString(noStringUnicode, reader, skip);

		uint32_t length = fileUtil::ReadFromFileBE<uint32_t>(reader);
		byteCount += GetPropertyFromLength(noString, length, reader, skip);

		const uint32_t numItems = fileUtil::ReadFromFileBE<uint32_t>(reader);

		for (uint32_t i = 0u; i < numItems; ++i)
		{
			byteCount += ParseProperty(reader, node.children, skip);
		}

		return byteCount + 8u;
	}

	void ResetCustomLayerData(Layer* layer)
	{
		layer->textData = nullptr;
	}

	void ResetCustomTextLayerData(TextLayerData* layerData)
	{
		layerData->fontColor[0] = -1.f;
		layerData->fontColor[1] = -1.f;
		layerData->fontColor[2] = -1.f;
		layerData->fontColor[3] = 1.f;

		layerData->transform[0] = 1.f;
		layerData->transform[1] = 0.f;
		layerData->transform[2] = 0.f;
		layerData->transform[3] = 1.f;
		layerData->transform[4] = 0.f;
		layerData->transform[5] = 0.f;

		layerData->fontSize = -1.f;
		layerData->leading = -1.f;
		layerData->baseline = -1.f;
		layerData->kerning = -1.f;
		layerData->fontStyles = nullptr;

		layerData->isAutoLeading = false;
		layerData->isAutoKerning = false;
		layerData->isBold = false;
		layerData->isItalic = false;
		layerData->isStrikethrough = false;
		layerData->isUnderlined = false;
	}

	void ParseCustomProperties(uint32_t length, const uint32_t key, Layer* layer, SyncFileReader& reader, Allocator* allocator)
	{
		if (key == util::Key<'T', 'y', 'S', 'h'>::VALUE)
		{
			const uint16_t version = fileUtil::ReadFromFileBE<uint16_t>(reader);

			layer->type = layerType::TEXT;
			layer->textData = memoryUtil::Allocate<TextLayerData>(allocator);

			for (uint8_t i = 0u; i < 6u; ++i)
			{
				layer->textData->transform[i] = fileUtil::ReadFromFileBE<float64_t>(reader);
			}

			reader.Skip(6u);

			uint32_t byteCount = 56u;

			TraversalTreeNode root;
			TraversalTreeNode textNode("Txt ");
			TraversalTreeNode textEngineNode("EngineData");

			textNode.textUtf16Callback = [&](std::wstring& unicodeString) -> void
				{
					layer->textData->utf16Text = memoryUtil::AllocateArray<uint16_t>(allocator, unicodeString.size());
					std::memcpy(layer->textData->utf16Text, unicodeString.data(), unicodeString.size() * sizeof(uint16_t));

					ResetCustomTextLayerData(layer->textData);
				};

			textEngineNode.textCallback = [&](std::string& rawDataString) -> void
				{
					ParseTextRawData(rawDataString, layer, allocator);
				};

			root.attach(textNode);
			root.attach(textEngineNode);

			byteCount += ParseObjectDescriptor(reader, root);

			reader.Skip(length - byteCount - 32u);

			if (layer->textData)
			{
				layer->textData->left = fileUtil::ReadFromFileBE<float64_t>(reader);
				layer->textData->top = fileUtil::ReadFromFileBE<float64_t>(reader);
				layer->textData->right = fileUtil::ReadFromFileBE<float64_t>(reader);
				layer->textData->bottom = fileUtil::ReadFromFileBE<float64_t>(reader);
			}
			else
			{
				reader.Skip(32u);
			}
		}
		else
		{
			reader.Skip(length);
		}
	}

	void DestroyAllocatedCustomData(Layer* layer, Allocator* allocator)
	{
		if (layer->textData)
		{
			memoryUtil::FreeArray(allocator, layer->textData->utf16Text);

			if (layer->textData->fontStyles)
			{
				int i = 0;
				while (layer->textData->fontStyles[i])
				{
					memoryUtil::FreeArray(allocator, layer->textData->fontStyles[i++]);
				}
			}
			memoryUtil::FreeArray(allocator, layer->textData->fontStyles);
		}
		memoryUtil::Free(allocator, layer->textData);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetFontSizeFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("FontSize");

		if (pos != std::string::npos)
		{
			pos += 8;
			std::string fontSize = rawDataString.substr(pos);
			std::istringstream iss(fontSize);
			float value;
			iss >> value;

			return value;
		}

		return -1.f;
	}

	float GetKerningFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("Kerning");

		if (pos != std::string::npos)
		{
			pos += 7;
			std::string baseline = rawDataString.substr(pos);
			std::istringstream iss(baseline);
			float value;
			iss >> value;

			return value;
		}

		return -1.f;
	}

	float GetBaselineOffsetFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("BaselineShift");

		if (pos != std::string::npos)
		{
			pos += 13;
			std::string baseline = rawDataString.substr(pos);
			std::istringstream iss(baseline);
			float value;
			iss >> value;

			return value;
		}

		return -1.f;
	}

	float GetLeadingFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("Leading");

		if (pos != std::string::npos)
		{
			pos += 7;
			std::string leading = rawDataString.substr(pos);
			std::istringstream iss(leading);
			float value;
			iss >> value;

			return value;
		}

		return -1.f;
	}

	bool GetAutoLeadingFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("AutoLeading");

		if (pos != std::string::npos)
		{
			pos += 11;
			std::string autoLeading = rawDataString.substr(pos);
			std::istringstream iss(autoLeading);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	bool GetAutoKerningFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("AutoKerning");

		if (pos != std::string::npos)
		{
			pos += 11;
			std::string autoKerning = rawDataString.substr(pos);
			std::istringstream iss(autoKerning);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	bool GetFauxBoldFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("FauxBold");

		if (pos != std::string::npos)
		{
			pos += 8;
			std::string bold = rawDataString.substr(pos);
			std::istringstream iss(bold);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	bool GetFauxItalicFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("FauxItalic");

		if (pos != std::string::npos)
		{
			pos += 10;
			std::string italic = rawDataString.substr(pos);
			std::istringstream iss(italic);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	bool GetUnderlineFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("Underline");

		if (pos != std::string::npos)
		{
			pos += 9;
			std::string underline = rawDataString.substr(pos);
			std::istringstream iss(underline);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	bool GetStrikethroughFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("Strikethrough");

		if (pos != std::string::npos)
		{
			pos += 13;
			std::string strikethrough = rawDataString.substr(pos);
			std::istringstream iss(strikethrough);
			bool value;
			iss >> std::boolalpha >> value;

			return value;
		}

		return false;
	}

	std::vector<float32_t> GetFillColorFromRawData(const std::string& rawDataString)
	{
		std::vector<float32_t> fillColor;
		fillColor.reserve(4);

		// Finding the position where to start the search
		size_t pos = rawDataString.find("FillColor");

		if (pos != std::string::npos)
		{
			pos += 9; // Start searching right after the specific string

			// Regular expression to find floating-point numbers
			std::regex pattern(R"([-+]?[0-9]*\.?[0-9]+)");
			// Searching for numbers starting from the found position
			std::sregex_iterator iter(rawDataString.begin() + pos, rawDataString.end(), pattern);
			std::sregex_iterator end;

			while ((iter != end) && (fillColor.size() < 4))
			{
				fillColor.push_back(std::stof((*(iter++)).str()));
			}
		}

		return fillColor;
	}

	std::vector<uint16_t*> GetFontStylesFromRawData(const std::string& rawDataString, Allocator* allocator)
	{
		std::vector<uint16_t*> fontStyles;

		std::string keyString = "FontSet";
		std::regex pattern(keyString + R"(.*?\[([^\]]+)\])");
		std::smatch matches;

		if (std::regex_search(rawDataString, matches, pattern) && matches.size() > 1)
		{
			// matches[1] contains the first captured group, which is the text inside the quotation marks
			std::string extractedText = matches[1].str();

			pattern = std::regex(R"(\(([^)]*)\))");

			std::string::const_iterator searchStart(extractedText.cbegin());
			while (std::regex_search(searchStart, extractedText.cend(), matches, pattern) && matches.size() > 1)
			{
				std::string extractedFont = matches[1].str().substr(2);

				std::string extractedFontName = matches[1].str();
				std::wstring unicodeText((const wchar_t*)extractedFontName.data(), extractedFontName.size() / 2);

				for (wchar_t& c : unicodeText)
				{
					c = (c >> 8) + (c << 8);
				}

				auto tempPtr = memoryUtil::AllocateArray<uint16_t>(allocator, unicodeText.size());
				std::memcpy(tempPtr, unicodeText.data(), (unicodeText.size() + 1) * sizeof(uint16_t));
				fontStyles.push_back(tempPtr);

				searchStart = matches.suffix().first;
			}

			if (fontStyles.size())
			{
				fontStyles.push_back(nullptr);
			}
		}

		return fontStyles;
	}

	int8_t GetJustificationFromRawData(const std::string& rawDataString)
	{
		size_t pos = rawDataString.find("Justification");

		if (pos != std::string::npos)
		{
			pos += 13;
			std::string justification = rawDataString.substr(pos);
			std::istringstream iss(justification);
			float value;
			iss >> value;

			return static_cast<int8_t>(value);
		}

		return 0;
	}

	void ParseTextRawData(const std::string& rawDataString, Layer* layer, Allocator* allocator)
	{
		/*		std::ofstream file("C:\\Users\\Andres\\Documents\\debug_output.txt");
				file << rawDataString;
				file.close();
		*/

		if (layer->textData)
		{
			float32_t fontSize = GetFontSizeFromRawData(rawDataString);
			if (fontSize > 0.f)
			{
				layer->textData->fontSize = fontSize;
			}

			float32_t kerning = GetKerningFromRawData(rawDataString);
			if (kerning > 0.f)
			{
				layer->textData->kerning = kerning;
			}

			float32_t leading = GetLeadingFromRawData(rawDataString);
			if (leading > 0.f)
			{
				layer->textData->leading = leading;
			}

			float32_t baseline = GetBaselineOffsetFromRawData(rawDataString);
			if (baseline > 0.f)
			{
				layer->textData->baseline = baseline;
			}

			layer->textData->justificationType = GetJustificationFromRawData(rawDataString);

			layer->textData->isAutoKerning = GetAutoKerningFromRawData(rawDataString);
			layer->textData->isAutoLeading = GetAutoLeadingFromRawData(rawDataString);
			layer->textData->isBold = GetFauxBoldFromRawData(rawDataString);
			layer->textData->isItalic = GetFauxItalicFromRawData(rawDataString);
			layer->textData->isUnderlined = GetUnderlineFromRawData(rawDataString);
			layer->textData->isStrikethrough = GetStrikethroughFromRawData(rawDataString);

			std::vector<float32_t> fillColor = GetFillColorFromRawData(rawDataString);

			if (fillColor.size())
			{
				std::memcpy(layer->textData->fontColor, fillColor.data(), fillColor.size() * sizeof(float32_t));
			}

			std::vector<uint16_t*> fontStyles = GetFontStylesFromRawData(rawDataString, allocator);

			if (fontStyles.size())
			{
				layer->textData->fontStyles = memoryUtil::AllocateArray<uint16_t*>(allocator, fontStyles.size());
				std::memcpy(layer->textData->fontStyles, fontStyles.data(), fontStyles.size() * sizeof(uint16_t*));
			}
		}
	}
}

PSD_NAMESPACE_END
