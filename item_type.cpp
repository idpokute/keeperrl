#include "stdafx.h"
#include "item_type.h"
#include "item.h"
#include "creature.h"
#include "equipment.h"
#include "furniture.h"
#include "attr_type.h"
#include "player_message.h"
#include "vision.h"
#include "body.h"
#include "view_object.h"
#include "attack_type.h"
#include "game.h"
#include "creature_factory.h"
#include "monster_ai.h"
#include "view_id.h"
#include "item_attributes.h"
#include "skill.h"
#include "technology.h"
#include "lasting_effect.h"
#include "name_generator.h"
#include "trap_type.h"
#include "furniture_type.h"
#include "resource_id.h"

ItemType::ItemType(const ItemType&) = default;
ItemType::ItemType(ItemType&) = default;
ItemType::ItemType(ItemType&&) = default;
ItemType::ItemType() {}

ItemType& ItemType::operator = (const ItemType&) = default;
ItemType& ItemType::operator = (ItemType&&) = default;


class FireScrollItem : public Item {
  public:
  FireScrollItem(const ItemAttributes& attr) : Item(attr) {}

  virtual void applySpecial(WCreature c) override {
    set = true;
  }

  virtual void specialTick(Position position) override {
    if (set) {
      fireDamage(0.03, position);
      set = false;
    }
  }

  SERIALIZE_ALL(SUBCLASS(Item), set)
  SERIALIZATION_CONSTRUCTOR(FireScrollItem)

  private:
  bool SERIAL(set) = false;
};

class Corpse : public Item {
  public:
  Corpse(const ViewObject& obj2, const ItemAttributes& attr, const string& rottenN,
      double rottingT, CorpseInfo info) :
      Item(attr),
      object2(obj2),
      rottingTime(rottingT),
      rottenName(rottenN),
      corpseInfo(info) {
  }

  virtual void applySpecial(WCreature c) override {
    WItem it = c->getWeapon();
    if (it && it->getAttackType() == AttackType::CUT) {
      c->you(MsgType::DECAPITATE, getTheName());
      setName("decapitated " + getName());
    } else {
      c->privateMessage("You need something sharp to decapitate the corpse.");
    }
  }

  virtual void specialTick(Position position) override {
    double time = position.getGame()->getGlobalTime();
    if (rottenTime == -1)
      rottenTime = time + rottingTime;
    if (time >= rottenTime && !rotten) {
      setName(rottenName);
      setViewObject(object2);
      corpseInfo.isSkeleton = true;
      rotten = true;
    } else {
      if (!rotten && getWeight() > 10 && Random.roll(20 + (rottenTime - time) / 10))
        Effect::emitPoisonGas(position, 0.3, false);
      if (getWeight() > 10 && !corpseInfo.isSkeleton &&
          !position.isCovered() && Random.roll(350)) {
        for (Position v : position.neighbors8(Random)) {
          PCreature vulture = CreatureFactory::fromId(CreatureId::VULTURE, TribeId::getPest(),
                    MonsterAIFactory::scavengerBird(v));
          if (v.canEnter(vulture.get())) {
            v.addCreature(std::move(vulture));
            v.globalMessage("A vulture lands near " + getTheName());
            rottenTime -= 40;
            break;
          }
        }
      }
    }
  }

  virtual optional<CorpseInfo> getCorpseInfo() const override {
    return corpseInfo;
  }

  SERIALIZE_ALL(SUBCLASS(Item), object2, rotten, rottenTime, rottingTime, rottenName, corpseInfo)
  SERIALIZATION_CONSTRUCTOR(Corpse)

  private:
  ViewObject SERIAL(object2);
  bool SERIAL(rotten) = false;
  double SERIAL(rottenTime) = -1;
  double SERIAL(rottingTime);
  string SERIAL(rottenName);
  CorpseInfo SERIAL(corpseInfo);
};

PItem ItemFactory::corpse(const string& name, const string& rottenName, double weight, ItemClass itemClass,
    CorpseInfo corpseInfo) {
  const double rotTime = 300;
  return makeOwner<Corpse>(
        ViewObject(ViewId::BONE, ViewLayer::ITEM, rottenName),
        ITATTR(
          i.viewId = ViewId::BODY_PART;
          i.name = name;
          i.shortName = name;
          i.itemClass = itemClass;
          i.weight = weight;),
        rottenName,
        rotTime,
        corpseInfo);
}

class PotionItem : public Item {
  public:
  PotionItem(const ItemAttributes& attr) : Item(attr) {}

  virtual void fireDamage(double amount, Position position) override {
    heat += amount;
    INFO << getName() << " heat " << heat;
    if (heat > 0.1) {
      position.globalMessage(getAName() + " boils and explodes!");
      discarded = true;
    }
  }

  virtual void specialTick(Position position) override {
    heat = max(0., heat - 0.005);
  }

  SERIALIZE_ALL(SUBCLASS(Item), heat)
  SERIALIZATION_CONSTRUCTOR(PotionItem)

  private:
  double SERIAL(heat) = 0;
};

class SkillBook : public Item {
  public:
  SkillBook(const ItemAttributes& attr, Skill* s) : Item(attr), skill(s->getId()) {}

  virtual void applySpecial(WCreature c) override {
    c->addSkill(Skill::get(skill));
  }

  SERIALIZE_ALL(SUBCLASS(Item), skill)
  SERIALIZATION_CONSTRUCTOR(SkillBook)

  private:
  SkillId SERIAL(skill);
};

class TechBookItem : public Item {
  public:
  TechBookItem(const ItemAttributes& attr, optional<TechId> t) : Item(attr), tech(t) {}

  virtual void applySpecial(WCreature c) override {
    if (!read || !!tech) {
      c->getGame()->addEvent(EventInfo::TechbookRead{tech ? Technology::get(*tech) : nullptr});
      read = true;
    }
  }

  SERIALIZE_ALL(SUBCLASS(Item), tech, read)
  SERIALIZATION_CONSTRUCTOR(TechBookItem)

  private:
  optional<TechId> SERIAL(tech);
  bool SERIAL(read) = false;
};

REGISTER_TYPE(SkillBook)
REGISTER_TYPE(TechBookItem)
REGISTER_TYPE(PotionItem)
REGISTER_TYPE(FireScrollItem)
REGISTER_TYPE(Corpse)


ItemAttributes ItemType::getAttributes() const {
  return type.visit([&](const auto& t) { return t.getAttributes(); });
}

PItem ItemType::get() const {
  return type.visit(
      [&](const FireScroll&) {
        return makeOwner<FireScrollItem>(getAttributes());
      },
      [&](const RandomTechBook&) {
        return makeOwner<TechBookItem>(getAttributes(), none);
      },
      [&](const TechBook& t) {
        return makeOwner<TechBookItem>(getAttributes(), t.techId);
      },
      [&](const Potion& t) {
        return makeOwner<PotionItem>(getAttributes());
      },
      [&](const auto&) {
        return makeOwner<Item>(this->getAttributes());
      }
  );
}


static int getEffectPrice(Effect type) {
  return type.visit(
      [&](const Effect::Lasting& e) {
        return LastingEffects::getPrice(e.lastingEffect);
      },
      [&](const Effect::Acid&) {
        return 8;
      },
      [&](const Effect::Heal&) {
        return 8;
      },
      [&](const Effect::Teleport&) {
        return 12;
      },
      [&](const Effect::Fire&) {
        return 12;
      },
      [&](const Effect::Alarm&) {
        return 12;
      },
      [&](const Effect::SilverDamage&) {
        return 12;
      },
      [&](const Effect::DestroyEquipment&) {
        return 12;
      },
      [&](const Effect::DestroyWalls&) {
        return 30;
      },
      [&](const Effect::EnhanceWeapon&) {
        return 12;
      },
      [&](const Effect::EnhanceArmor&) {
        return 12;
      },
      [&](const Effect::TeleEnemies&) {
        return 12;
      },
      [&](const Effect::CurePoison&) {
        return 12;
      },
      [&](const Effect::Summon&) {
        return 12;
      },
      [&](const Effect::EmitPoisonGas&) {
        return 20;
      },
      [&](const Effect::Deception&) {
        return 30;
      },
      [&](const Effect::CircularBlast&) {
        return 30;
      },
      [&](const Effect::PlaceFurniture&) {
        return 30;
      },
      [&](const Effect::SummonElement&) {
        return 30;
      },
      [&](const Effect::Damage&) {
        return 30;
      },
      [&](const Effect::InjureBodyPart&) {
        return 30;
      },
      [&](const Effect::LooseBodyPart&) {
        return 30;
      },
      [&](const Effect::RegrowBodyPart&) {
        return 30;
      }
  );
}

ViewId getRingViewId(LastingEffect e) {
  switch (e) {
    case LastingEffect::FIRE_RESISTANT: return ViewId::FIRE_RESIST_RING;
    case LastingEffect::POISON_RESISTANT: return ViewId::POISON_RESIST_RING;
    default: return ViewId::FIRE_RESIST_RING;
  }
}

ViewId getAmuletViewId(LastingEffect e) {
  switch (e) {
    case LastingEffect::REGENERATION: return ViewId::AMULET1;
    case LastingEffect::WARNING: return ViewId::AMULET2;
    default: return ViewId::AMULET3;
  }
}

static int maybePlusMinusOne(int prob) {
  if (Random.roll(prob))
    return Random.get(2) * 2 - 1;
  return 0;
}

static const vector<pair<string, vector<string>>> badArtifactNames {
  {"sword", { "bang", "crush", "fist"}},
  {"battle axe", {"crush", "tooth", "razor", "fist", "bite", "bolt", "sword"}},
  {"war hammer", {"blade", "tooth", "bite", "bolt", "sword", "steel"}}};

static void makeArtifact(ItemAttributes& i) {
  bool good;
  do {
    good = true;
    i.artifactName = NameGenerator::get(NameGeneratorId::WEAPON)->getNext();
    for (auto elem : badArtifactNames)
      for (auto pattern : elem.second)
        if (contains(toLower(*i.artifactName), pattern) && contains(*i.name, elem.first)) {
          INFO << "Rejected artifact " << *i.name << " " << *i.artifactName;
          good = false;
        }
  } while (!good);
  INFO << "Making artifact " << *i.name << " " << *i.artifactName;
  i.modifiers[AttrType::DAMAGE] += Random.get(1, 4);
  i.price *= 15;
}

enum class WeaponPrefix { SILVER, FLAMING, POISONOUS, GREAT, LEAD_FILLED };

static void addPrefix(ItemAttributes& i, WeaponPrefix prefix) {
  i.price *= 7;
  switch (prefix) {
    case WeaponPrefix::SILVER:
      i.name = "silver " + *i.name;
      if (i.plural)
        i.plural = "silver " + *i.plural;
      i.attackEffect = Effect(Effect::SilverDamage{});
      break;
    case WeaponPrefix::FLAMING:
      i.name = "flaming " + *i.name;
      if (i.plural)
        i.plural = "flaming " + *i.plural;
      i.attackEffect = Effect(Effect::Fire{});
      break;
    case WeaponPrefix::POISONOUS:
      i.name = "poisonous " + *i.name;
      if (i.plural)
        i.plural = "poisonous " + *i.plural;
      i.attackEffect = Effect(Effect::Lasting{LastingEffect::POISON});
      break;
    case WeaponPrefix::GREAT:
      i.name = "great " + *i.name;
      if (i.plural)
        i.plural = "great " + *i.plural;
      i.attackEffect = Effect(Effect::Lasting{LastingEffect::BLEEDING});
      break;
    case WeaponPrefix::LEAD_FILLED:
      i.name = "lead-filled " + *i.name;
      if (i.plural)
        i.plural = "lead-filled " + *i.plural;
      i.attackEffect = Effect(Effect::Lasting{LastingEffect::COLLAPSED});
      break;
  }
}

vector<PItem> ItemType::get(int num) const {
  vector<PItem> ret;
  for (int i : Range(num))
    ret.push_back(get());
  return ret;
}

ItemAttributes ItemType::AutomatonItem::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::TRAP_ITEM;
      i.shortName = "automaton"_s;
      i.name = "automaton";
      i.applyMsgFirstPerson = "assemble the automaton"_s;
      i.applyMsgThirdPerson = "assembles an automaton"_s;
      i.applySound = SoundId::TRAP_ARMING;
      i.weight = 30;
      i.itemClass = ItemClass::TOOL;
      i.description = "";
      i.applyTime = 3;
      i.uses = 1;
      i.price = 60;
      i.effect = Effect(Effect::Summon{CreatureId::AUTOMATON});
  );
}

ItemAttributes ItemType::Knife::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::KNIFE;
      i.name = "knife";
      i.plural = "knives"_s;
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 0.3;
      i.modifiers[AttrType::DAMAGE] = 5 + maybePlusMinusOne(4);
      i.attackTime = 0.7;
      i.price = 1;
      i.attackType = AttackType::STAB;
  );
}

ItemAttributes ItemType::SpecialKnife::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::KNIFE;
      i.name = "knife";
      i.plural = "knives"_s;
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 0.3;
      i.modifiers[AttrType::DAMAGE] = 5 + maybePlusMinusOne(4);
      i.attackTime = 0.7;
      i.price = 1;
      i.attackType = AttackType::STAB;
      addPrefix(i, WeaponPrefix::POISONOUS);
      makeArtifact(i);
  );
}

ItemAttributes ItemType::Spear::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPEAR;
      i.name = "spear";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.5;
      i.modifiers[AttrType::DAMAGE] = 10 + maybePlusMinusOne(4);
      i.price = 4;
      i.attackType = AttackType::STAB;
  );
}

ItemAttributes ItemType::Sword::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SWORD;
      i.name = "sword";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.5;
      i.modifiers[AttrType::DAMAGE] = 8 + maybePlusMinusOne(4);
      i.price = 4;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::SpecialSword::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPECIAL_SWORD;
      i.name = "sword";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.5;
      i.modifiers[AttrType::DAMAGE] = 8 + maybePlusMinusOne(4);
      i.price = 4;
      i.attackType = AttackType::CUT;
      addPrefix(i, WeaponPrefix::FLAMING);
      makeArtifact(i);
  );
}

ItemAttributes ItemType::SteelSword::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::STEEL_SWORD;
      i.name = "steel sword";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.2;
      i.modifiers[AttrType::DAMAGE] = 11 + maybePlusMinusOne(4);
      i.price = 20;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::ElvenSword::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::ELVEN_SWORD;
      i.name = "elven sword";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1;
      i.modifiers[AttrType::DAMAGE] = 9 + maybePlusMinusOne(4);
      i.price = 8;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::SpecialElvenSword::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPECIAL_SWORD;
      i.name = "elven sword";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1;
      i.modifiers[AttrType::DAMAGE] = 9 + maybePlusMinusOne(4);
      i.price = 8;
      i.attackType = AttackType::CUT;
      addPrefix(i, WeaponPrefix::SILVER);
      makeArtifact(i);
  );
}

ItemAttributes ItemType::BattleAxe::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::BATTLE_AXE;
      i.name = "battle axe";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 8;
      i.modifiers[AttrType::DAMAGE] = 14 + maybePlusMinusOne(4);
      i.attackTime = 1.2;
      i.twoHanded = true;
      i.price = 30;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::SpecialBattleAxe::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPECIAL_BATTLE_AXE;
      i.name = "battle axe";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 8;
      i.modifiers[AttrType::DAMAGE] = 14 + maybePlusMinusOne(4);
      i.attackTime = 1.2;
      i.twoHanded = true;
      i.price = 30;
      i.attackType = AttackType::CUT;
      addPrefix(i, WeaponPrefix::GREAT);
      makeArtifact(i);
  );
}

ItemAttributes ItemType::SteelBattleAxe::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::STEEL_BATTLE_AXE;
      i.name = "steel battle axe";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 7;
      i.modifiers[AttrType::DAMAGE] = 18 + maybePlusMinusOne(4);
      i.attackTime = 1.2;
      i.twoHanded = true;
      i.price = 150;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::WarHammer::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::WAR_HAMMER;
      i.name = "war hammer";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 8;
      i.modifiers[AttrType::DAMAGE] = 12 + maybePlusMinusOne(4);
      i.attackTime = 1.2;
      i.twoHanded = true;
      i.price = 20;
      i.attackType = AttackType::CRUSH;
  );
}

ItemAttributes ItemType::SpecialWarHammer::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPECIAL_WAR_HAMMER;
      i.name = "war hammer";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 8;
      i.modifiers[AttrType::DAMAGE] = 12 + maybePlusMinusOne(4);
      i.attackTime = 1.2;
      i.twoHanded = true;
      i.price = 20;
      i.attackType = AttackType::CRUSH;
      addPrefix(i, WeaponPrefix::LEAD_FILLED);
      makeArtifact(i);
  );
}

ItemAttributes ItemType::Club::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::CLUB;
      i.name = "club";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 2;
      i.modifiers[AttrType::DAMAGE] = 4 + maybePlusMinusOne(4);
      i.price = 2;
      i.attackType = AttackType::CRUSH;
  );
}

ItemAttributes ItemType::HeavyClub::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::HEAVY_CLUB;
      i.name = "heavy club";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 8;
      i.modifiers[AttrType::DAMAGE] = 10 + maybePlusMinusOne(4);
      i.twoHanded = true;
      i.price = 4;
      i.attackType = AttackType::CRUSH;
  );
}

ItemAttributes ItemType::WoodenStaff::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::WOODEN_STAFF;
      i.name = "wooden staff";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.5;
      i.modifiers[AttrType::SPELL_DAMAGE] = 4 + maybePlusMinusOne(4);
      i.meleeAttackAttr = AttrType::SPELL_DAMAGE;
      i.price = 30;
      i.attackType = AttackType::SPELL;
  );
}

ItemAttributes ItemType::IronStaff::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::IRON_STAFF;
      i.name = "iron staff";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 1.5;
      i.modifiers[AttrType::SPELL_DAMAGE] = 8 + maybePlusMinusOne(4);
      i.meleeAttackAttr = AttrType::SPELL_DAMAGE;
      i.price = 60;
      i.attackType = AttackType::SPELL;
  );
}

ItemAttributes ItemType::Scythe::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SWORD;
      i.name = "scythe";
      i.itemClass = ItemClass::WEAPON;
      i.equipmentSlot = EquipmentSlot::WEAPON;
      i.weight = 5;
      i.modifiers[AttrType::DAMAGE] = 12 + maybePlusMinusOne(4);
      i.twoHanded = true;
      i.price = 20;
      i.attackType = AttackType::CUT;
  );
}

ItemAttributes ItemType::ElvenBow::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::ELVEN_BOW;
      i.itemClass = ItemClass::RANGED_WEAPON;
      i.equipmentSlot = EquipmentSlot::RANGED_WEAPON;
      i.rangedWeapon = RangedWeapon(AttrType::RANGED_DAMAGE, "arrow", ViewId::ARROW);
      i.twoHanded = true;
      i.weight = 1;
      i.modifiers[AttrType::RANGED_DAMAGE] = 16;
      i.name = "silver elven bow";
      i.price = 100;
  );
}

ItemAttributes ItemType::Bow::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::BOW;
      i.name = "short bow";
      i.itemClass = ItemClass::RANGED_WEAPON;
      i.equipmentSlot = EquipmentSlot::RANGED_WEAPON;
      i.rangedWeapon = RangedWeapon(AttrType::RANGED_DAMAGE, "arrow", ViewId::ARROW);
      i.twoHanded = true;
      i.weight = 1;
      i.modifiers[AttrType::RANGED_DAMAGE] = 10 + maybePlusMinusOne(4);
      i.price = 12;
  );
}

ItemAttributes ItemType::Robe::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::ROBE;
      i.name = "robe";
      i.shortName = "robe"_s;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BODY_ARMOR;
      i.weight = 2;
      i.price = 10;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::LeatherGloves::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::LEATHER_GLOVES;
      i.shortName = "leather"_s;
      i.name = "pair of leather gloves";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::GLOVES;
      i.weight = 0.3;
      i.price = 2;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::StrengthGloves::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::STRENGTH_GLOVES;
      i.shortName = "strength"_s;
      i.name = "gloves of " + *i.shortName;
      i.plural = "pairs of " + *i.name;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::GLOVES;
      i.weight = 0.3;
      i.price = 25;
      i.modifiers[AttrType::DAMAGE] = 2 + maybePlusMinusOne(4);;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::LeatherArmor::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::LEATHER_ARMOR;
      i.shortName = "leather"_s;
      i.name = "leather armor";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BODY_ARMOR;
      i.weight = 7;
      i.price = 4;
      i.modifiers[AttrType::DEFENSE] = 3 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::LeatherHelm::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::LEATHER_HELM;
      i.shortName = "leather"_s;
      i.name = "leather helm";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::HELMET;
      i.weight = 1.5;
      i.price = 1;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::ChainArmor::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::CHAIN_ARMOR;
      i.shortName = "chain"_s;
      i.name = "chain armor";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BODY_ARMOR;
      i.weight = 15;
      i.price = 25;
      i.modifiers[AttrType::DEFENSE] = 5 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::SteelArmor::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::STEEL_ARMOR;
      i.shortName = "steel"_s;
      i.name = "steel armor";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BODY_ARMOR;
      i.weight = 13;
      i.price = 160;
      i.modifiers[AttrType::DEFENSE] = 8 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::IronHelm::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::IRON_HELM;
      i.shortName = "iron"_s;
      i.name = "iron helm";
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::HELMET;
      i.weight = 4;
      i.price = 8;
      i.modifiers[AttrType::DEFENSE]= 2 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::TelepathyHelm::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::TELEPATHY_HELM;
      i.shortName = "telepathy"_s;
      i.name = "helm of " + *i.shortName;
      i.plural = "helms of " + *i.shortName;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::HELMET;
      i.equipedEffect = LastingEffect::TELEPATHY;
      i.weight = 1.5;
      i.price = 70;
      i.modifiers[AttrType::DEFENSE]= 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::LeatherBoots::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::LEATHER_BOOTS;
      i.shortName = "leather"_s;
      i.name = "pair of leather boots";
      i.plural = "pairs of leather boots"_s;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BOOTS;
      i.weight = 2;
      i.price = 2;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::IronBoots::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::IRON_BOOTS;
      i.shortName = "iron"_s;
      i.name = "pair of iron boots";
      i.plural = "pairs of iron boots"_s;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BOOTS;
      i.weight = 4;
      i.price = 8;
      i.modifiers[AttrType::DEFENSE] = 2 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::SpeedBoots::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SPEED_BOOTS;
      i.shortName = "speed"_s;
      i.name = "boots of " + *i.shortName;
      i.plural = "pairs of boots of " + *i.shortName;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BOOTS;
      i.weight = 2;
      i.price = 70;
      i.modifiers[AttrType::SPEED] = 30;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::LevitationBoots::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::LEVITATION_BOOTS;
      i.shortName = "levitation"_s;
      i.equipedEffect = LastingEffect::FLYING;
      i.name = "boots of " + *i.shortName;
      i.plural = "pairs of boots of " + *i.shortName;
      i.itemClass = ItemClass::ARMOR;
      i.equipmentSlot = EquipmentSlot::BOOTS;
      i.weight = 2;
      i.price = 70;
      i.modifiers[AttrType::DEFENSE] = 1 + maybePlusMinusOne(4);
  );
}

ItemAttributes ItemType::Ring::getAttributes() const {
  return ITATTR(
      i.viewId = getRingViewId(lastingEffect);
      i.shortName = string(LastingEffects::getName(lastingEffect));
      i.equipedEffect = lastingEffect;
      i.name = "ring of " + *i.shortName;
      i.plural = "rings of " + *i.shortName;
      i.description = string(LastingEffects::getDescription(lastingEffect));
      i.weight = 0.05;
      i.equipmentSlot = EquipmentSlot::RINGS;
      i.itemClass = ItemClass::RING;
      i.price = 40;
  );
}

ItemAttributes ItemType::Amulet::getAttributes() const {
  return ITATTR(
      i.viewId = getAmuletViewId(lastingEffect);
      i.shortName = string(LastingEffects::getName(lastingEffect));
      i.equipedEffect = lastingEffect;
      i.name = "amulet of " + *i.shortName;
      i.plural = "amulets of " + *i.shortName;
      i.description = string(LastingEffects::getDescription(lastingEffect));
      i.itemClass = ItemClass::AMULET;
      i.equipmentSlot = EquipmentSlot::AMULET;
      i.price = 5 * LastingEffects::getPrice(lastingEffect);
      i.weight = 0.3;
  );
}

ItemAttributes ItemType::DefenseAmulet::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::AMULET3;
      i.shortName = "defense"_s;
      i.name = "amulet of " + *i.shortName;
      i.plural = "amulets of " + *i.shortName;
      i.description = "Increases the toughness of your skin and flesh, making you harder to wound.";
      i.itemClass = ItemClass::AMULET;
      i.equipmentSlot = EquipmentSlot::AMULET;
      i.price = 60;
      i.modifiers[AttrType::DEFENSE] = 3 + maybePlusMinusOne(4);
      i.weight = 0.3;
  );
}

ItemAttributes ItemType::FirstAidKit::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::FIRST_AID;
      i.name = "first aid kit";
      i.weight = 0.5;
      i.itemClass = ItemClass::TOOL;
      i.description = "Heals your wounds, but requires a few turns to apply.";
      i.applyTime = 3;
      i.uses = Random.get(3, 6);
      i.usedUpMsg = true;
      i.displayUses = true;
      i.price = 2;
      i.effect = {Effect::Heal{}};
  );
}

static FurnitureType getTrapFurniture(TrapType type) {
  switch (type) {
    case TrapType::ALARM:
      return FurnitureType::ALARM_TRAP;
    case TrapType::BOULDER:
      return FurnitureType::BOULDER_TRAP;
    case TrapType::POISON_GAS:
      return FurnitureType::POISON_GAS_TRAP;
    case TrapType::SURPRISE:
      return FurnitureType::SURPRISE_TRAP;
    case TrapType::TERROR:
      return FurnitureType::TERROR_TRAP;
    case TrapType::WEB:
      return FurnitureType::WEB_TRAP;
  }
}

ItemAttributes ItemType::TrapItem::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::TRAP_ITEM;
      auto trapName = getTrapName(trapType);
      i.name = "unarmed " + trapName + " trap";
      i.shortName = trapName;
      i.weight = 0.5;
      i.itemClass = ItemClass::TOOL;
      i.applyTime = 3;
      i.applySound = SoundId::TRAP_ARMING;
      i.uses = 1;
      i.usedUpMsg = true;
      i.trapType = trapType;
      i.effect = Effect(Effect::PlaceFurniture{getTrapFurniture(trapType)});
      i.price = 2;
  );
}

ItemAttributes ItemType::Potion::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::POTION1;
      i.shortName = effect.getName();
      i.name = "potion of " + *i.shortName;
      i.plural = "potions of " + *i.shortName;
      i.description = effect.getDescription();
      i.blindName = "potion"_s;
      i.itemClass = ItemClass::POTION;
      i.fragile = true;
      i.weight = 0.3;
      i.effect = effect;
      i.price = getEffectPrice(effect);
      i.flamability = 0.3;
      i.uses = 1;
  );
}

static ViewId getMushroomViewId(Effect e) {
  return e.visit(
      [&](const Effect::Lasting& e) {
        switch (e.lastingEffect) {
          case LastingEffect::DAM_BONUS: return ViewId::MUSHROOM1;
          case LastingEffect::DEF_BONUS: return ViewId::MUSHROOM2;
          case LastingEffect::PANIC: return ViewId::MUSHROOM3;
          case LastingEffect::HALLU: return ViewId::MUSHROOM4;
          case LastingEffect::RAGE: return ViewId::MUSHROOM5;
          default: return ViewId::MUSHROOM6;
        }
      },
      [&](const auto&) {
        return ViewId::MUSHROOM6;
      }
  );
}

ItemAttributes ItemType::Mushroom::getAttributes() const {
  return ITATTR(
      i.viewId = getMushroomViewId(effect);
      i.shortName = effect.getName();
      i.name = *i.shortName + " mushroom";
      i.blindName = "mushroom"_s;
      i.description = effect.getDescription();
      i.itemClass= ItemClass::FOOD;
      i.weight = 0.1;
      i.modifiers[AttrType::DAMAGE] = -15;
      i.effect = effect;
      i.price = getEffectPrice(effect);
      i.uses = 1;
  );
}

ItemAttributes ItemType::Scroll::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SCROLL;
      i.shortName = effect.getName();
      i.name = "scroll of " + *i.shortName;
      i.plural= "scrolls of "  + *i.shortName;
      i.description = effect.getDescription();
      i.blindName = "scroll"_s;
      i.itemClass = ItemClass::SCROLL;
      i.weight = 0.1;
      i.modifiers[AttrType::DAMAGE] = -10;
      i.effect = effect;
      i.price = getEffectPrice(effect);
      i.flamability = 1;
      i.uses = 1;
  );
}

ItemAttributes ItemType::FireScroll::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::SCROLL;
      i.name = "scroll of fire";
      i.plural= "scrolls of fire"_s;
      i.shortName = "fire"_s;
      i.description = "Sets itself on fire.";
      i.blindName = "scroll"_s;
      i.itemClass= ItemClass::SCROLL;
      i.weight = 0.1;
      i.modifiers[AttrType::DAMAGE] = -10;
      i.price = 15;
      i.flamability = 1;
      i.uses = 1;
  );
}

ItemAttributes ItemType::TechBook::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::BOOK;
      i.shortName = Technology::get(techId)->getName();
      i.name = "book of " + *i.shortName;
      i.plural = "books of " + *i.shortName;
      i.weight = 1;
      i.itemClass = ItemClass::BOOK;
      i.applyTime = 3;
      i.price = 1000;
  );
}

ItemAttributes ItemType::RandomTechBook::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::BOOK;
      i.name = "book of knowledge";
      i.plural = "books of knowledge"_s;
      i.weight = 0.5;
      i.itemClass = ItemClass::BOOK;
      i.applyTime = 3;
      i.price = 300;
  );
}

ItemAttributes ItemType::Rock::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::ROCK;
      i.name = "rock";
      i.itemClass = ItemClass::OTHER;
      i.price = 0;
      i.resourceId = CollectiveResourceId::STONE;
      i.weight = 0.3;
  );
}

ItemAttributes ItemType::IronOre::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::IRON_ROCK;
      i.name = "iron ore";
      i.itemClass = ItemClass::OTHER;
      i.price = 0;
      i.resourceId = CollectiveResourceId::IRON;
      i.weight = 0.5;
  );
}

ItemAttributes ItemType::SteelIngot::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::STEEL_INGOT;
      i.name = "steel ingot";
      i.itemClass = ItemClass::OTHER;
      i.price = 0;
      i.resourceId = CollectiveResourceId::STEEL;
      i.weight = 0.5;
  );
}

ItemAttributes ItemType::WoodPlank::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::WOOD_PLANK;
      i.name = "wood plank";
      i.itemClass = ItemClass::OTHER;
      i.price = 0;
      i.resourceId = CollectiveResourceId::WOOD;
      i.weight = 5;
  );
}

ItemAttributes ItemType::Bone::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::BONE;
      i.name = "bone";
      i.itemClass = ItemClass::OTHER;
      i.price = 0;
      i.weight = 5;
  );
}

ItemAttributes ItemType::GoldPiece::getAttributes() const {
  return ITATTR(
      i.viewId = ViewId::GOLD;
      i.name = "gold piece";
      i.itemClass = ItemClass::GOLD;
      i.price = 1;
      i.resourceId = CollectiveResourceId::GOLD;
      i.weight = 0.01;
  );
}

SERIALIZE_DEF(ItemType, type)

#include "pretty_archive.h"
template void ItemType::serialize(PrettyInputArchive&, unsigned);
