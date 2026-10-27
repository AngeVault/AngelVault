#pragme once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


class Layer
{
private:
    std::string m_name;
    char m_layerLevel;
    bool m_isShowed = true;
public:
    Layer(std::string name, char layerLevel);
    bool hideAndShow();
    void changeName(std::string name);
    std::string getName() { return m_name; }
};

enum class Layers{DESTROY = 0, DISPLAY, NAME};
class LayerManager
{
private:
    std::vector<Layer> m_layers;
public:
    LayerManager();
    void addLayer(std::string name, char drawingOrder);
    void modifyLayer(int index, Layers instruction, std::string name = "");
    int getLayerIndex(std::string name);
};