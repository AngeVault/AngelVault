#include "Layer.h"


Layer::Layer(std::string name, char layerLevel)
    :m_name(name), m_layerLevel(layerLevel)
{
}

bool Layer::hideAndShow()
{
    m_isSowed = !m_isSowed;
    return m_isSowed;
}

void Layer::changeName(std::string name)
{
    m_name = name;
}


void LayerManager::addLayer(std::string name, char layerLevel)
{
    m_layers.push_back(Layer(name, layerLevel));
}

void modifyLayer(int index, Layers instruction, std::string name = "")
{
    switch (instruction)
    {
        case Layers::DESTROY:
            for (int i = m_layers.size()-1; i > index; i--)
            {
                m_layers[i-1] = m_layers[i];
            }
            m_layers.pop_back();
            return;
        case Layers::DISPLAY:
            layers[index].hideAndShow();
            return;
        case Layers::NAME:
            if (name == "")return;
            layers[index].changeName(name);
            return;
    }

}
int LayerManager::getLayerIndex(std::string name)
{
    for (int i = 0; i < m_layers.size()-1; i++)
    {
        if (m_layers.getName() == name)
            return i;
    }
}
