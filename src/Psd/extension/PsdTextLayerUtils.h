#pragma once
#include "../PsdNamespace.h"
#include "../PsdKey.h"
#include "../PsdSyncFileReader.h"
#include "../PsdTypes.h"

#include <string>
#include <unordered_map>
#include <functional>

PSD_NAMESPACE_BEGIN

struct Layer;
class Allocator;

namespace textLayers
{
	using namespace std;

	enum JUSTIFICATION_TYPE : int8_t
	{
		JUST_RIGHT = 0,
		JUST_LEFT = 1,
		JUST_CENTERED = 2
	};

	struct TextLayerData
	{
		uint16_t* utf16Text;
		uint16_t** fontStyles;
		float32_t fontColor[4];
		float64_t transform[6];

		float32_t fontSize;
		float32_t kerning;
		float32_t leading;
		float32_t baseline;

		float64_t top;
		float64_t bottom;
		float64_t left;
		float64_t right;

		uint32_t orientation;

		int8_t justificationType;

		bool isAutoKerning = false;
		bool isAutoLeading = false;
		bool isBold = false;
		bool isItalic = false;
		bool isUnderlined = false;
		bool isStrikethrough = false;
	};

	struct TraversalTreeNode
	{
		unordered_map<string, TraversalTreeNode> children;
		string propertyName = "";
		function<void()> callback;
		function<void(wstring&)> textUtf16Callback;
		function<void(string&)> textCallback;
		function<void(float64_t)> longCallback;

		TraversalTreeNode() = default;
		TraversalTreeNode(string inPropertyName) : propertyName(inPropertyName) {};

		void attach(const TraversalTreeNode childNode)
		{
			children[childNode.propertyName] = childNode;
		}
	};

	uint32_t GetPropertyFromLength(string& property, uint32_t length, SyncFileReader& reader, bool skip = false);

	uint32_t GetUnicodeString(wstring& readableString, SyncFileReader& reader, bool skip = false);

	uint32_t ParseObjectDescriptor(SyncFileReader& reader, TraversalTreeNode& node, bool skip = false);

	uint32_t ParseProperty(SyncFileReader& reader, unordered_map<string, TraversalTreeNode>& propertyTree, bool skip = false);

	void ResetCustomLayerData(Layer* layer);

	void ResetCustomTextLayerData(TextLayerData* layerData);

	void ParseCustomProperties(uint32_t length, const uint32_t key, Layer* layer, SyncFileReader& reader, Allocator* allocator);

	void DestroyAllocatedCustomData(Layer* layer, Allocator* allocator);

	// Raw data
	// TODO (Andres): This parsing is extremely simple and lacks a structure lookup. Could be more robust.
	float GetFontSizeFromRawData(const std::string& rawDataString);

	float GetKerningFromRawData(const std::string& rawDataString);

	float GetBaselineOffsetFromRawData(const std::string& rawDataString);

	float GetLeadingFromRawData(const std::string& rawDataString);

	bool GetAutoLeadingFromRawData(const std::string& rawDataString);

	bool GetAutoKerningFromRawData(const std::string& rawDataString);

	bool GetFauxBoldFromRawData(const std::string& rawDataString);

	bool GetFauxItalicFromRawData(const std::string& rawDataString);

	bool GetUnderlineFromRawData(const std::string& rawDataString);

	bool GetStrikethroughFromRawData(const std::string& rawDataString);

	std::vector<float32_t> GetFillColorFromRawData(const std::string& rawDataString);

	std::vector<uint16_t*> GetFontStylesFromRawData(const std::string& rawDataString, Allocator* allocator);

	int8_t GetJustificationFromRawData(const std::string& rawDataString);

	void ParseTextRawData(const std::string& rawDataString, Layer* layer, Allocator* allocator);
}

PSD_NAMESPACE_END
