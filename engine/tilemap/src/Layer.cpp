#include "Layer.h"

#include <iostream>
#include <ostream>
#include <stdexcept>


Layer::Layer(const std::string& name, const LayersType layerType)
    :m_name(name), m_layerType(layerType)
{
}

bool Layer::hideAndShow()
{
    m_isShowed = !m_isShowed;
    return m_isShowed;
}

void Layer::changeName(const std::string& name)
{
    m_name = name;
}


void LayerManager::addLayer(std::string name, const LayersType layerType)
{
    m_layers.push_back(Layer(name, layerType));
}

void LayerManager::modifyLayer(const int index, const Layers instruction, const std::string& name = "")
{
    switch (instruction)
    {
        case Layers::DESTROY:
            if (index < 0 || index >= m_layers.size())return;
            for (int i = m_layers.size()-1; i > index; i--)
            {
                m_layers[i-1] = m_layers[i];
            }
            m_layers.pop_back();
            return;
        case Layers::DISPLAY:
            m_layers[index].hideAndShow();
            return;
        case Layers::NAME:
            if (name == "")return;
            m_layers[index].changeName(name);
            return;
    }

}
int LayerManager::getLayerIndex(const std::string& name)
{
    for (int i = 0; i < m_layers.size()-1; i++)
    {
        if (m_layers[i].getName() == name)
            return i;
    }
}

Layer& LayerManager::getLayer(const int index) {
    try {
        if (index < 0 || index > m_layers.size()) throw std::out_of_range("");
        return m_layers[index];
    } catch (std::out_of_range& e) {
        std::cerr << e.what() << std::endl;
    }

}
