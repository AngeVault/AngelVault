#pragma once
#include <algorithm>
#include <string>
#include <vector>

// une case : un type d'objet et sa quantité
struct ItemStack {
    std::string item;
    int count = 1;
};

// objets portés par une entité, rangés par cases
struct Inventory {
    int capacity = 20; // 0 : illimité
    int maxStack = 99; // 0 : illimité
    std::vector<ItemStack> stacks;

    // renvoie ce qui n'a pas tenu
    int add(const std::string& item, int count) {
        if (item.empty() || count <= 0) return std::max(count, 0);
        for (ItemStack& s : stacks) {
            if (s.item != item) continue;
            const int put = std::min(count, room(s));
            s.count += put;
            count -= put;
            if (count == 0) return 0;
        }
        while (count > 0 && (capacity <= 0 || int(stacks.size()) < capacity)) {
            const int put = maxStack > 0 ? std::min(count, maxStack) : count;
            stacks.push_back({ item, put });
            count -= put;
        }
        return count;
    }

    // renvoie ce qui a été retiré, en vidant d'abord les dernières cases
    int remove(const std::string& item, int count) {
        int removed = 0;
        for (auto it = stacks.rbegin(); it != stacks.rend() && removed < count; ++it) {
            if (it->item != item) continue;
            const int take = std::min(count - removed, it->count);
            it->count -= take;
            removed += take;
        }
        std::erase_if(stacks, [](const ItemStack& s) { return s.count <= 0; });
        return removed;
    }

    int count(const std::string& item) const {
        int n = 0;
        for (const ItemStack& s : stacks) if (s.item == item) n += s.count;
        return n;
    }

    bool has(const std::string& item, int n = 1) const { return count(item) >= n; }

    // combien de cet objet peut encore entrer
    int space(const std::string& item) const {
        if (capacity <= 0 || maxStack <= 0) return 1 << 30;
        int n = (capacity - int(stacks.size())) * maxStack;
        for (const ItemStack& s : stacks) if (s.item == item) n += room(s);
        return n;
    }

private:
    int room(const ItemStack& s) const { return maxStack > 0 ? std::max(maxStack - s.count, 0) : 1 << 30; }
};
