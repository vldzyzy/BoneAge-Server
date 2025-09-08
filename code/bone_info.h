#pragma once

#include <string>
#include <string_view>
#include <array>
#include <memory>
#include <stdexcept>
#include <optional>

struct BoneInfo {

    BoneInfo() = delete;

    // struct Bone {
    //     std::optional<std::string_view> name; // key joint 才有名字
    //     const std::string_view detect_class;
    //     const std::string_view cls_class;
    //     const int maturity_range;
    // };

    struct BoneDetectInfo {
        const int id;
        const std::string_view name;
        const int expected_detect_count;
    };

    struct BoneClsInfo {
        const int id;
        const std::string_view name;
        const int maturity_range;
    };

    static constexpr std::array<BoneDetectInfo, 7> kDetectBones = {{
        {0, "Radius", 1},
        {1, "Ulna", 1},
        {2, "FMCP", 1},
        {3, "MCP", 4},
        {4, "PIP", 5},
        {5, "MIP", 4},
        {6, "DIP", 5}
    }};

    static constexpr std::array<BoneClsInfo, 9> kClsBones = {{
        {0, "Radius",    14},
        {1, "Ulna",      12},
        {2, "MCPFirst",  11},
        {3, "MCP",       10},
        {4, "PIPFirst",  12}, // 检测任务中属于PIP
        {5, "PIP",       12},
        {6, "MIP",       12},
        {7, "DIPFirst",  11}, // 检测任务中属于DIP
        {8, "DIP",       11},
    }};

    static constexpr std::array<std::string_view, 13> kKeyJoints = {
        "radius",
        "ulna",
        "mcpfirst",
        "mcpthird",
        "mcpfifth", 
        "pipfirst",  
        "pipthird",   
        "pipfifth",    
        "mipthird",   
        "mipfifth",  
        "dipfirst",   
        "dipthird",  
        "dipfifth"
    };

    static constexpr std::string_view DetectGetNameById(int id) {
        for (const auto& item : kDetectBones) {
            if (item.id == id) {
                return item.name;
            }
        }
        throw std::out_of_range("Invalid Detect Class ID"); 
    }

    static constexpr int DetectGetExpectedCountById(int id) {
        for (const auto& item : kDetectBones) {
            if (item.id == id) {
                return item.expected_detect_count;
            }
        }
        throw std::out_of_range("Invalid Detect Class ID"); 
    }

    static constexpr std::string_view ClsGetNameById(int id) {
        for (const auto& category : kClsBones) {
            if (category.id == id) {
                return category.name;
            }
        }
        throw std::out_of_range("Invalid Category ID"); 
    }

    static constexpr int ClsGetIdByName(std::string_view name) {
        for (const auto& category : kClsBones) {
            if (category.name == name) {
                return category.id;
            }
        }
        throw std::out_of_range("Invalid Category name");
    }

    static constexpr std::string_view JointGetNameById(int id) {
        if (id >= 0 && id <= kKeyJoints.size()) {
            return kKeyJoints[id];
        }
        throw std::out_of_range("Invalid Joint ID"); 
    }

    static constexpr int JointGetIdByName(std::string_view name) {
        for (int i = 0; i < kKeyJoints.size(); i++) {
            if (kKeyJoints[i] == name) {
                return i;
            }
        }
        throw std::out_of_range("Invalid Joint name");
    }
    
    static constexpr int GetMaturityRange(int id) {
        for (const auto& category : kClsBones) {
            if (category.id == id) {
                return category.maturity_range;
            }
        }
        throw std::out_of_range("Invalid bone ID");
    }
};

