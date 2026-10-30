#pragma once
#include <string>
#include <vector>


enum class LayersType{BACK_GROUND = 0, FLOOR, BUILDING};
enum class Layers{DESTROY = 0, DISPLAY, NAME};
class Layer
{
private:
    std::string m_name;
    LayersType m_layerType;
    bool m_isShowed = true;
public:
    Layer(const std::string& name, LayersType layerType);
    bool hideAndShow();
    void changeName(const std::string& name);
    std::string getName() { return m_name; }
};

class LayerManager
{
private:
    std::vector<Layer> m_layers;
public:
    LayerManager() = default;
    void addLayer(std::string name, LayersType layerType);
    void modifyLayer(int index, Layers instruction, const std::string& name);
    int getLayerIndex(const std::string& name);
    int getSize() const { return m_layers.size(); }
    Layer& getLayer(int index);
};