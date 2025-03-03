#include "Chef.hpp"
#include "../config.hpp"
#include "../toolEquipped.hpp"
#include "Calculator.hpp"
#include "exception.hpp"
#include "include/json/json.h"
#include "loadToolEquipped.hpp"
#include "utils/json.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <string>

CookAbility Chef::globalAbilityBuff;
int Chef::globalAbilityMale = 0;
int Chef::globalAbilityFemale = 0;
std::map<int, Skill> Skill::skillList;
void initBuff(const Json::Value usrBuff) {
    Chef::setGlobalBuff(CookAbility(usrBuff));
    Chef::setGlobalAbilityMale(getInt(usrBuff["Male"]));
    Chef::setGlobalAbilityFemale(getInt(usrBuff["Female"]));
    Chef::setGlobalAbilityAll(getInt(usrBuff["All"]));
}
void splitUltimateSkill(std::map<int, int> &ultimateSkills,
                        const Json::Value &ids) {
    for (auto pair : ids) {
        auto str = pair.asString();
        int id = atoi(str.substr(0, str.find(",")).c_str());
        int skillId = atoi(str.substr(str.find(",") + 1).c_str());
        ultimateSkills[id] = skillId;
    }
}
void loadUltimateSkills(std::map<int, int> &ultimateSkills,
                        const Json::Value &usrBuff) {
    splitUltimateSkill(ultimateSkills, usrBuff["Partial"]["id"]);
    splitUltimateSkill(ultimateSkills, usrBuff["Self"]["id"]);
}
void loadChef(CList &chefList, const Json::Value &usrData,
              const Json::Value &gameData) {
    if (MODE == 2) {
        Chef::coinBuffOn = false;
    } else {
        Chef::coinBuffOn = true;
    }

    initBuff(usrData["userUltimate"]);
    const Json::Value chefs = gameData["chefs"];
    Skill::loadJson(gameData["skills"]);
    std::map<int, int> ultimateSkills;
    loadUltimateSkills(ultimateSkills, usrData["userUltimate"]);

    size_t numChefRecorded = 0;
    auto chefGot = usrData["chefGot"];
    for (auto chef : chefs) {
        int id = chef["chefId"].asInt();
        if (chefGot[std::to_string(id)].asBool()) {
            numChefRecorded++;
            if (AVOID_CHEF_1 && chef["rarity"].asInt() == 1) {
                continue;
            }
            if (AVOID_CHEF_2 && chef["rarity"].asInt() == 2) {
                continue;
            }
            if (AVOID_CHEF_3 && chef["rarity"].asInt() == 3) {
                continue;
            }

            if (ultimateSkills.find(id) != ultimateSkills.end()) {
                chefList.push_back(Chef(chef, ultimateSkills[id]));
            } else {
                chefList.push_back(Chef(chef, -1));
            }
        }
    }
    size_t numChefsGot = 0;
    for (auto &chef : chefGot) {
        if (chef.asBool()) {
            numChefsGot++;
        }
    }
    int delta = numChefsGot - numChefRecorded;
    if (delta > 0) {
        std::cout << RED "data.min.json已过期，缺了" << delta
                  << "个已有厨师，建议重新下载bcjh.zip。" NO_FORMAT
                  << std::endl;
    }

#ifdef _WIN32
    auto t = loadToolFile();
    if (t == EMPTY_FILE__NOT_EQUIPPED) {
        std::cout << "toolEquipped.csv没有设定规则。允许所有厨师装备厨具。"
                  << std::endl;
    } else if (t == NO_FILE__NO_TOOL) {
        std::cout << "未找到toolEquipped.csv文件。不会装配任何厨具。"
                  << std::endl;
    } else {
        std::cout << "toolEquipped.csv文件已加载。" << std::endl;
    }
    CSVWarning(w);
    for (auto &chef : chefList) {
        w += loadToolFromFile(&chef, t);
    }
    if (w.missingRarity3) {
        std::cout
            << "提示：当前版本toolEquipped."
               "csv已支持“制作三火料理售价加成”，详见样例，但读取到的toolEquipp"
               "ed.csv中没有这一项。默认“制作三火料理售价加成”均为0。"
            << std::endl;
    }
#endif
#ifdef __linux__
    for (auto &chef : chefList) {
        toolEquipped(&chef);
    }
#endif
}
void Chef::deletePointers() {
    delete this->name;
    delete this->skill;
    delete this->companyBuff;
    delete this->nextBuff;
}
/**
 * Chef
 * @param ultimateSkillId: -1 means no ultimate skill
 */
Chef::Chef(Json::Value &chef, int ultimateSkillId) {
    if (chef.isMember("chefId") && chef.isMember("name") &&
        chef.isMember("skill")) {
        // std::cout << "333" << std::endl;
        this->name = new std::string();
        this->skill = new Skill();
        this->companyBuff = new Skill();
        this->nextBuff = new Skill();
        this->id = chef["chefId"].asInt();
        *this->name = chef["name"].asString();
        this->skill->ability = CookAbility(chef);
        if (chef.isMember("tags") && chef["tags"].isArray()) {
            auto tags = chef["tags"];
            this->male = false;
            this->female = false;
            for (auto tag : tags) {
                if (tag.asInt() == 1) {
                    this->male = true;
                    this->skill->ability.add(globalAbilityMale);
                }
                if (tag.asInt() == 2) {
                    this->female = true;
                    this->skill->ability.add(globalAbilityFemale);
                }
            }
        }
    } else {
        std::cout << chef << std::endl;
        throw std::logic_error("Chef Json Error");
    }
    this->skill->ability.add(globalAbilityBuff);
    this->addSkill(chef["skill"].asInt());
    if (ultimateSkillId != -1) {
        this->addSkill(ultimateSkillId);
    }
    this->tool = NOT_EQUIPPED;
}

void Chef::print() const {
    std::cout << this->id << ": " << *this->name << "\t"
              << (this->male ? "M" : "") << (this->female ? "F" : "")
              << std::endl;
    // this->skill->print();
}
CookAbility::CookAbility(const Json::Value &v) {
    // std::cout << "Here" << std::endl;
    if (v.isMember("stirfry") && v.isMember("bake") && v.isMember("boil") &&
        v.isMember("fry") && v.isMember("knife")) {
        // std::cout << "yes1" << std::endl;
        this->stirfry = getInt(v["stirfry"]);
        this->bake = getInt(v["bake"]);
        this->boil = getInt(v["boil"]);
        this->steam = getInt(v["steam"]);
        this->fry = getInt(v["fry"]);
        this->knife = getInt(v["knife"]);

    } else if (v.isMember("Stirfry") && v.isMember("Bake") &&
               v.isMember("Boil") && v.isMember("Fry") && v.isMember("Knife")) {
        // std::cout << v << std::endl;
        this->stirfry = getInt(v["Stirfry"]);
        this->bake = getInt(v["Bake"]);
        this->boil = getInt(v["Boil"]);
        this->steam = getInt(v["Steam"]);
        this->fry = getInt(v["Fry"]);
        this->knife = getInt(v["Knife"]);
    } else {
        std::cout << "no" << std::endl;
        throw std::logic_error("CookAbility: Invalid Json");
    }
}
void Skill::loadJson(const Json::Value &v) {
    for (auto skill : v) {
        int id = skill["skillId"].asInt();
        skillList[id] = Skill();
        for (auto effect : skill["effect"]) {

            Skill skill;
            std::string condition = effect["condition"].asString();
            if (condition != "Global") {
                if (condition == "Partial") {
                    skillList[id].type = PARTIAL;
                } else if (condition == "Self") {
                    skillList[id].type = SELF;
                } else if (condition == "Next") {
                    skillList[id].type = NEXT;
                }
                std::string type = effect["type"].asString();
                int value = effect["value"].asInt();
                if (type == "Gold_Gain") {
                    skill.pricePercentBuff = value;
                } else if (type == "Stirfry") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.stirfry = value;
                    else {
                        skill.cookAbilityPercentBuff.stirfry = value;
                    }
                } else if (type == "Bake") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.bake = value;
                    else {
                        skill.cookAbilityPercentBuff.bake = value;
                    }
                } else if (type == "Boil") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.boil = value;
                    else {
                        skill.cookAbilityPercentBuff.boil = value;
                    }
                } else if (type == "Steam") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.steam = value;
                    else {
                        skill.cookAbilityPercentBuff.steam = value;
                    }

                } else if (type == "Fry") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.fry = value;
                    else {
                        skill.cookAbilityPercentBuff.fry = value;
                    }
                } else if (type == "Knife") {
                    std::string cal = effect["cal"].asString();
                    if (cal == "Abs")
                        skill.ability.knife = value;
                    else {
                        skill.cookAbilityPercentBuff.knife = value;
                    }
                } else if (type == "UseStirfry") {
                    skill.abilityBuff.stirfry = value;
                } else if (type == "UseBake") {
                    skill.abilityBuff.bake = value;
                } else if (type == "UseBoil") {
                    skill.abilityBuff.boil = value;
                } else if (type == "UseSteam") {
                    skill.abilityBuff.steam = value;
                } else if (type == "UseFry") {
                    skill.abilityBuff.fry = value;
                } else if (type == "UseKnife") {
                    skill.abilityBuff.knife = value;
                } else if (type == "UseSweet") {
                    skill.flavorBuff.sweet = value;
                } else if (type == "UseSour") {
                    skill.flavorBuff.sour = value;
                } else if (type == "UseSalty") {
                    skill.flavorBuff.salty = value;
                } else if (type == "UseBitter") {
                    skill.flavorBuff.bitter = value;
                } else if (type == "UseSpicy") {
                    skill.flavorBuff.spicy = value;
                } else if (type == "UseTasty") {
                    skill.flavorBuff.tasty = value;
                } else if (type == "UseVegetable") {
                    skill.materialBuff.vegetable = value;
                } else if (type == "UseMeat") {
                    skill.materialBuff.meat = value;
                } else if (type == "UseFish") {
                    skill.materialBuff.fish = value;
                } else if (type == "UseCreation") {
                    skill.materialBuff.creation = value;
                } else if (type == "BasicPrice"){
                    skill.baseAddBuff = value;
                }
                    BuffCondition *condition = NULL;
                if (effect.isMember("conditionType")) {

                    auto conditionType = effect["conditionType"].asString();
                    int cvalue = 0;
                    if (effect.isMember("conditionValue"))
                        cvalue = getInt(effect["conditionValue"]);
                    if (conditionType == "CookbookRarity") {
                        for (auto &i : effect["conditionValueList"]) {
                            skill.rarityBuff[getInt(i)] = value;
                        }
                    } else if (conditionType == "PerRank") {
                        condition = new GradeBuffCondition(cvalue);
                    } else if (conditionType == "ExcessCookbookNum") {
                        int rarityUpperBound = Recipe::upperBound(cvalue);
                        for (int i = 1; i <= rarityUpperBound; i++) {
                            skill.rarityBuff[i] = value;
                        }

                    } else if (conditionType == "SameSkill") {
                        condition = new ThreeSameCookAbilityBuffCondition();
                    } else if (conditionType == "Rank") {
                        skill.gradeBuff[cvalue] = value;
                    }
                }
                if (condition == NULL) {
                    skillList[id] += skill;
                } else {
                    skillList[id].conditionalEffects.push_back(
                        new ConditionalBuff(condition, skill));
                }
            }
        }
    }
}
void Chef::addSkill(int id) {
    auto skill = Skill::skillList[id];
    if (skill.type == Skill::SELF) {
        *this->skill += skill;
    } else if (skill.type == Skill::PARTIAL) {
        *this->companyBuff += skill;
    } else if (skill.type == Skill::NEXT) {
        *this->nextBuff += skill;
    }
}

// int CookAbility::operator/(const Ability &a) const {
//     int grade = 5;
//     const int *thisptr = &this->stirfry;
//     const int *aptr = &a.stirfry;
//     for (int i = 0; i < 6; i++) {
//         if (aptr[i] != 0) {
//             grade =
//                 grade < (thisptr[i] / aptr[i]) ? grade : (thisptr[i] /
//                 aptr[i]);
//         }
//     }
//     return grade;
// }
int CookAbility::operator*(const AbilityBuff &a) const {
    int buff = 0;
    const int *thisptr = &this->stirfry;
    const int *aptr = &a.stirfry;
    for (int i = 0; i < 6; i++) {
        if (thisptr[i] != 0) {
            buff += aptr[i];
        }
    }
    return buff;
}

// Chef Chef::addTool_modify_name(ToolEnum a) {
//     Chef newChef(*this);
//     newChef.tool = a;
//     switch (a) {
//     case STIRFRY:
//         newChef.skill.ability.stirfry += 100;
//         newChef.name += "-炒";
//         return newChef;
//     case BAKE:
//         newChef.skill.ability.bake += 100;
//         newChef.name += "-烤";
//         return newChef;
//     case STEAM:
//         newChef.skill.ability.steam += 100;
//         newChef.name += "-蒸";
//         return newChef;
//     case FRY:
//         newChef.skill.ability.fry += 100;
//         newChef.name += "-炸";
//         return newChef;
//     case BOIL:
//         newChef.skill.ability.boil += 100;
//         newChef.name += "-煮";
//         return newChef;
//     case KNIFE:
//         newChef.skill.ability.knife += 100;
//         newChef.name += "-切";
//         return newChef;
//     default:
//         std::cout << "Invalid Tool!" << std::endl;
//         throw 0;
//     }
// }
// bool Chef::isCapable(Recipe *recipe) {
//     if (this->skill.ability / recipe->cookAbility > 0) {
//         return true;
//     }
//     return false;
// }
// void Chef::loadRecipeCapable(std::vector<Recipe> &recipeList) {
//     if (this->tool == NO_TOOL) {
//         for (auto &recipe : recipeList) {
//             if (this->isCapable(&recipe)) {
//                 this->recipeCapable.push_back(&recipe);
//             }
//         }
//     } else if (this->tool == NOT_EQUIPPED) {
//         std::vector<Recipe *> recipeListCopy;
//         for (auto &recipe : recipeList) {
//             recipeListCopy.push_back(&recipe);
//         }
//         for (int i = 0; i < 6; i++) {
//             this->modifyTool((ToolEnum)i);
//             auto iter = recipeListCopy.begin();
//             while (iter != recipeListCopy.end()) {
//                 if (this->isCapable(*iter)) {
//                     this->recipeCapable.push_back(*iter);
//                     iter = recipeListCopy.erase(iter);
//                 } else {
//                     iter++;
//                 }
//             }
//         }
//         this->modifyTool(NOT_EQUIPPED);
//     }
// }
void Chef::modifyTool(ToolEnum a) {
    if (this->tool == NO_TOOL)
        return;
    this->tool = a;
}

std::string getToolName(ToolEnum tool) {
    std::string toolName;
    switch (tool) {
    case STIRFRY:
        toolName = "炒";
        break;
    case BOIL:
        toolName = "煮";
        break;
    case FRY:
        toolName = "炸";
        break;
    case STEAM:
        toolName = "蒸";
        break;
    case BAKE:
        toolName = "烤";
        break;
    case KNIFE:
        toolName = "切";
        break;
    case NO_TOOL:
        toolName = "设定厨具";
        break;
    default:
        toolName = "无厨具";
    }
    return toolName;
}