#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <algorithm>
#include <chrono>
#include <random>
#include <functional>
#include <limits>
#include <numeric>
#include <cmath>

using namespace std;

enum Stat { HEALTH=0, MELEE=1, GRENADE=2, SUPER=3, CLASS=4, WEAPONS=5 };
const int STAT_COUNT = 6;
const vector<string> STAT_NAMES = {"Health","Melee","Grenade","Super","Class","Weapons"};

enum Archetype { GRENADIER=0, BRAWLER=1, BULWARK=2, PARAGON=3, SPECIALIST=4 };
const vector<string> ARCH_NAMES = {"Grenadier","Brawler","Bulwark","Paragon","Specialist"};

enum AbilityChoice { NONE=0, MINI_MASTERWORK=1, PRIMARY_BOOST=2 };
const vector<string> ABILITY_NAMES = {
    "None",
    "Mini-Masterwork (+3 to 3 lowest stats)",
    "Primary Boost (+5 main/-5 another)"
};

struct ArchetypeDef { 
    int primary; 
    int sec1;
};

map<Archetype, ArchetypeDef> arche_defs;
array<int, STAT_COUNT> global_target;

void init_archetypes() {
    arche_defs[GRENADIER] = {GRENADE, SUPER};
    arche_defs[BRAWLER]   = {MELEE, HEALTH};
    arche_defs[BULWARK]   = {HEALTH, CLASS};
    arche_defs[PARAGON]   = {SUPER, MELEE};
    arche_defs[SPECIALIST]= {CLASS, WEAPONS};
}

int choose_third_stat(Archetype a, mt19937& rng) {
    ArchetypeDef def = arche_defs[a];
    vector<int> possible_stats;
    vector<double> weights;
    
    for (int i = 0; i < STAT_COUNT; i++) {
        if (i != def.primary && i != def.sec1) {
            possible_stats.push_back(i);
            weights.push_back(global_target[i] + 1);
        }
    }
    
    if (accumulate(weights.begin(), weights.end(), 0.0) == 0.0) {
        weights.assign(possible_stats.size(), 1.0);
    }
    
    discrete_distribution<int> dist(weights.begin(), weights.end());
    return possible_stats[dist(rng)];
}

array<int,STAT_COUNT> build_piece_stats(Archetype a, bool masterworked, AbilityChoice ability, int minus_stat, mt19937& rng) {
    array<int,STAT_COUNT> stats{}; stats.fill(0);
    ArchetypeDef def = arche_defs[a];
    int sec2 = choose_third_stat(a, rng);
    
    stats[def.primary] = 30;
    stats[def.sec1] = 25;
    stats[sec2] = 20;

    if (masterworked) {
        vector<pair<int,int>> stat_order;
        for (int i = 0; i < STAT_COUNT; i++) stat_order.emplace_back(stats[i], i);
        sort(stat_order.begin(), stat_order.end());
        for (int i = 0; i < 3; i++) stats[stat_order[i].second] += 5;
    }

    if (ability == MINI_MASTERWORK) {
        vector<pair<int,int>> stat_order;
        for (int i = 0; i < STAT_COUNT; i++) stat_order.emplace_back(stats[i], i);
        sort(stat_order.begin(), stat_order.end());
        for (int i = 0; i < 3; i++) stats[stat_order[i].second] += 3;
    }
    else if (ability == PRIMARY_BOOST) {
        stats[def.primary] += 5;
        if (minus_stat >= 0 && minus_stat < STAT_COUNT && minus_stat != def.primary) {
            stats[minus_stat] = max(0, stats[minus_stat] - 5);
        }
    }

    return stats;
}

array<int,STAT_COUNT> sum_stats(const vector<array<int,STAT_COUNT>>& pieces) {
    array<int,STAT_COUNT> total{}; total.fill(0);
    for (const auto& piece : pieces) {
        for (int i = 0; i < STAT_COUNT; i++) {
            total[i] += piece[i];
        }
    }
    return total;
}

array<int,STAT_COUNT> apply_mods(const array<int,STAT_COUNT>& stats, const array<int,STAT_COUNT>& mods) {
    array<int,STAT_COUNT> result;
    for (int i = 0; i < STAT_COUNT; i++) {
        result[i] = stats[i] + mods[i] * 10;
    }
    return result;
}

void find_best_combo(const array<int,STAT_COUNT>& target, int max_mods = 5) {
    cout << "\nOptimizing...\n";
    random_device rd;
    mt19937 rng(rd());
    global_target = target;

    vector<Archetype> best_combo(5);
    vector<AbilityChoice> best_abilities(5);
    vector<int> best_minus_stats(5, -1);
    array<int,STAT_COUNT> best_mods{};
    array<int,STAT_COUNT> best_stats{};
    double best_score = numeric_limits<double>::max();

    vector<array<int,STAT_COUNT>> mod_distributions;
    array<int,STAT_COUNT> current_mods{};
    
    function<void(int,int)> generate_mods = [&](int pos, int remaining) {
        if (pos == STAT_COUNT - 1) {
            current_mods[pos] = remaining;
            mod_distributions.push_back(current_mods);
            return;
        }
        for (int i = 0; i <= remaining; i++) {
            current_mods[pos] = i;
            generate_mods(pos + 1, remaining - i);
        }
    };
    generate_mods(0, max_mods);

    const int TOTAL_ATTEMPTS = 25000;
    for (int attempt = 0; attempt < TOTAL_ATTEMPTS; attempt++) {
        vector<Archetype> combo(5);
        vector<AbilityChoice> abilities(5);
        vector<int> minus_stats(5, -1);
        vector<array<int,STAT_COUNT>> pieces(5);

        for (int i = 0; i < 5; i++) {
            combo[i] = static_cast<Archetype>(rng() % 5);
            ArchetypeDef def = arche_defs[combo[i]];
            
            bool should_prioritize_pb = false;
            if (target[def.primary] > 0) {
                int current_primary = best_stats.empty() ? 0 : best_stats[def.primary];
                if (current_primary < target[def.primary]) {
                    should_prioritize_pb = true;
                }
                else if (target[def.primary] >= *max_element(target.begin(), target.end()) * 0.8) {
                    should_prioritize_pb = true;
                }
            }
            
            if (should_prioritize_pb) {
                abilities[i] = (rng() % 100 < 80) ? PRIMARY_BOOST : MINI_MASTERWORK;
            } else {
                abilities[i] = (rng() % 100 < 30) ? PRIMARY_BOOST : MINI_MASTERWORK;
            }
            
            if (abilities[i] == PRIMARY_BOOST) {
                vector<pair<int, int>> stat_priorities;
                
                for (int j = 0; j < STAT_COUNT; j++) {
                    if (j == def.primary) continue;
                    
                    int priority = 0;
                    if (target[j] == 0) priority += 100;
                    else if (!best_stats.empty() && best_stats[j] > target[j] + 15) priority += 50;
                    else if (target[j] < *max_element(target.begin(), target.end()) * 0.5) priority += 20;
                    
                    stat_priorities.emplace_back(j, priority);
                }
                
                sort(stat_priorities.begin(), stat_priorities.end(),
                    [](const pair<int,int>& a, const pair<int,int>& b) {
                        return a.second > b.second;
                    });
                
                if (!stat_priorities.empty()) {
                    int choices = min(3, (int)stat_priorities.size());
                    minus_stats[i] = stat_priorities[rng() % choices].first;
                } else {
                    minus_stats[i] = -1;
                    abilities[i] = MINI_MASTERWORK;
                }
            }
            
            pieces[i] = build_piece_stats(combo[i], true, abilities[i], minus_stats[i], rng);
        }

        auto base_stats = sum_stats(pieces);

        for (const auto& mods : mod_distributions) {
            auto total = apply_mods(base_stats, mods);
            
            double score = 0;
            
            for (int i = 0; i < 5; i++) {
                ArchetypeDef def = arche_defs[combo[i]];
                if (target[def.primary] > 0) {
                    int deficit = max(0, target[def.primary] - total[def.primary]);
                    score += deficit * (abilities[i] == PRIMARY_BOOST ? 1.8 : 2.0);
                }
            }
            
            for (int i = 0; i < STAT_COUNT; i++) {
                bool is_primary = false;
                for (int j = 0; j < 5; j++) {
                    if (arche_defs[combo[j]].primary == i) {
                        is_primary = true;
                        break;
                    }
                }
                
                if (!is_primary && target[i] > 0) {
                    int deficit = max(0, target[i] - total[i]);
                    score += deficit * 1.5;
                }
            }

            double ability_score = 0;
            for (int i = 0; i < 5; i++) {
                ArchetypeDef def = arche_defs[combo[i]];
                if (abilities[i] == PRIMARY_BOOST && target[def.primary] > 0) {
                    double importance = target[def.primary] / (double)*max_element(target.begin(), target.end());
                    ability_score -= 3.0 * importance;
                }
                else if (abilities[i] != PRIMARY_BOOST && target[def.primary] > 0) {
                    int deficit = max(0, target[def.primary] - total[def.primary]);
                    if (deficit > 5) {
                        ability_score += deficit * 0.5;
                    }
                }
            }

            int total_stats = accumulate(total.begin(), total.end(), 0);
            score -= total_stats * 0.01;

            if (score < best_score) {
                best_score = score;
                best_combo = combo;
                best_abilities = abilities;
                best_minus_stats = minus_stats;
                best_mods = mods;
                best_stats = total;
                
            }
        }
        if (best_score <= -20.0) break;
    }

    cout << "\n=== BEST ARMOR SETUP ===\n";
    for (int i = 0; i < 5; i++) {
        cout << "Piece " << i+1 << ": " << ARCH_NAMES[best_combo[i]] << " - ";
        cout << ABILITY_NAMES[best_abilities[i]];
        if (best_abilities[i] == PRIMARY_BOOST && best_minus_stats[i] != -1) {
            cout << " (minus " << STAT_NAMES[best_minus_stats[i]] << ")";
        }
        cout << "\n";
    }

    cout << "\n=== MODS APPLIED ===\n";
    int total_mods = 0;
    for (int i = 0; i < STAT_COUNT; i++) {
        if (best_mods[i] > 0) {
            cout << "+" << best_mods[i]*10 << " " << STAT_NAMES[i] << "\n";
            total_mods += best_mods[i];
        }
    }
    cout << "Total mods used: " << total_mods << "/" << max_mods << "\n";

    cout << "\n=== FINAL STATS ===\n";
    int total_stats = 0;
    for (int i = 0; i < STAT_COUNT; i++) {
        cout << STAT_NAMES[i] << ": " << best_stats[i];
        if (target[i] > 0) {
            if (best_stats[i] >= target[i]) {
                cout << " (MET)";
            } else {
                cout << " (Missing " << target[i] - best_stats[i] << ")";
            }
        }
        cout << "\n";
        total_stats += best_stats[i];
    }
    cout << "TOTAL STATS: " << total_stats << "\n";
}

int main() {
    init_archetypes();
    cout << "Destiny 2 Armor Optimizer\n\n";
    cout << "Enter desired stat targets (0 = ignore):\n";

    array<int,STAT_COUNT> target;
    for (int i = 0; i < STAT_COUNT; i++) {
        while (true) {
            cout << STAT_NAMES[i] << ": ";
            if (cin >> target[i] && target[i] >= 0 && target[i] <= 200) break;
            cout << "Please enter a value between 0-200\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    find_best_combo(target);

    cout << "\nOptimization complete. Press Enter to exit...";
    cin.ignore();
    cin.get();
    return 0;
}