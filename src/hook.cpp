#include "hook.h"

namespace hooks
{
	void OnMeleeHitHook::Set_iFrames(RE::Actor* actor)
	{
		actor->SetGraphVariableBool("bIframeActive", true);
		actor->SetGraphVariableBool("bInIframe", true);
	}

	void OnMeleeHitHook::Reset_iFrames(RE::Actor* actor)
	{
		actor->SetGraphVariableBool("bIframeActive", false);
		actor->SetGraphVariableBool("bInIframe", false);
	}

	void OnMeleeHitHook::dispelEffect(RE::MagicItem *spellForm, RE::Actor *a_actor)
	{
		if (const auto targetActor = a_actor->AsMagicTarget(); targetActor)
		{
			if (spellForm && spellForm->effects[0] && spellForm->effects[0]->baseEffect && targetActor->HasMagicEffect(spellForm->effects[0]->baseEffect))
			{
				if (auto activeEffects = targetActor->GetActiveEffectList(); activeEffects)
				{
					for (const auto &effect : *activeEffects)
					{
						if (effect && effect->spell && effect->spell == spellForm)
						{
							effect->Dispel(true);
						}
					}
				}
			}
		}
	}

	bool Has_Magiceffect_Keyword(const RE::Actor *a_actor, const RE::BGSKeyword *a_key)
	{
		static RE::TESConditionItem cond;
		static std::once_flag flag;
		std::call_once(flag, [&]()
					   {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasMagicEffectKeyword;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
		cond.data.object                = RE::CONDITIONITEMOBJECT::kSelf;
        cond.data.comparisonValue.f     = 0.0f; });

		ConditionParam cond_param;
		cond_param.form = const_cast<RE::BGSKeyword *>(a_key->As<RE::BGSKeyword>());
		cond.data.functionData.params[0] = std::bit_cast<void *>(cond_param);

		RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>()), nullptr);
		return cond(params);
	}

	bool HasBoundWeaponEquipped(const RE::Actor *a_actor, RE::MagicSystem::CastingSource type)
	{
		static RE::TESConditionItem cond;
		static std::once_flag flag;
		std::call_once(flag, [&]()
					   {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kHasBoundWeaponEquipped;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        cond.data.object                = RE::CONDITIONITEMOBJECT::kSelf;
        cond.data.comparisonValue.f     = 0.0f; });

		ConditionParam cond_param;
		cond_param.i = static_cast<int32_t>(type);
		cond.data.functionData.params[0] = std::bit_cast<void *>(cond_param);

		RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>()), nullptr);
		return cond(params);
	}

	bool GetLineOfSight(const RE::Actor *a_actor, const RE::Actor *a_target, float a_comparison_value)
	{
		static RE::TESConditionItem cond;
		static std::once_flag flag;
		std::call_once(flag, [&]()
					   {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetLineOfSight;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        cond.data.comparisonValue.f     = a_comparison_value; });

		ConditionParam cond_param;
		cond_param.form = const_cast<RE::TESObjectREFR *>(a_target->As<RE::TESObjectREFR>());
		cond.data.functionData.params[0] = std::bit_cast<void *>(cond_param);

		RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>()),
										const_cast<RE::TESObjectREFR *>(a_target->As<RE::TESObjectREFR>()));
		return cond(params);
	}

	bool GetIsGhost(const RE::Actor *a_actor, float a_comparison_value)
	{
		static RE::TESConditionItem cond;
		static std::once_flag flag;
		std::call_once(flag, [&]()
					   {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetIsGhost;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
		cond.data.object                = RE::CONDITIONITEMOBJECT::kSelf;
        cond.data.comparisonValue.f     = a_comparison_value; });

		RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>()), nullptr);
		return cond(params);
	}

	float OnMeleeHitHook::GetActorValuePercent(RE::Actor *a_actor, RE::ActorValue a_value)
	{
		auto result = 0.0f;
		auto cur_value = a_actor->AsActorValueOwner()->GetActorValue(a_value);
		auto perm_value = a_actor->AsActorValueOwner()->GetPermanentActorValue(a_value);

		if (perm_value != 0.0f)
		{
			result = cur_value / perm_value;
		}
		return result;
	}

	bool OnMeleeHitHook::isHumanoid(RE::Actor* a_actor)
	{
		auto bodyPartData = a_actor->GetRace() ? a_actor->GetRace()->bodyPartData : nullptr;
		return bodyPartData && bodyPartData->GetFormID() == 0x1d;
	}

	

	void OnMeleeHitHook::UnequipAll(RE::Actor* a_actor)
	{
		uniqueLocker lock(mtx_Inventory);
		auto         itt = _Inventory.find(a_actor);
		if (itt == _Inventory.end()) {
			std::vector<RE::TESBoundObject*> Hen;
			_Inventory.insert({ a_actor, Hen });
		}

		for (auto it = _Inventory.begin(); it != _Inventory.end(); ++it) {
			if (it->first == a_actor) {
				auto inv = a_actor->GetInventory();
				for (auto& [item, data] : inv) {
					const auto& [count, entry] = data;
					if (count > 0 && entry->IsWorn()) {
						RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, item);
						it->second.push_back(item);
					}
				}
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::Re_EquipAll(RE::Actor* a_actor)
	{
		uniqueLocker lock(mtx_Inventory);
		for (auto it = _Inventory.begin(); it != _Inventory.end(); ++it) {
			if (it->first == a_actor) {
				for (auto item : it->second) {
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, item);
				}
				_Inventory.erase(it);
				break;
			}
			continue;
		}
	}

	bool OnMeleeHitHook::isPowerAttacking(RE::Actor* a_actor)
	{
		auto currentProcess = a_actor->GetActorRuntimeData().currentProcess;
		if (currentProcess) {
			auto highProcess = currentProcess->high;
			if (highProcess) {
				auto attackData = highProcess->attackData;
				if (attackData) {
					auto flags = attackData->data.flags;
					return flags.any(RE::AttackData::AttackFlag::kPowerAttack);
				}
			}
		}
		return false;
	}

	void OnMeleeHitHook::UpdateCombatTarget(RE::Actor* a_actor){
		auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		if (!CTarget) {
			auto combatGroup = a_actor->GetCombatGroup();
			if (combatGroup) {
				for (auto it = combatGroup->targets.begin(); it != combatGroup->targets.end(); ++it) {
					if (it->targetHandle && it->targetHandle.get().get()) {
						a_actor->GetActorRuntimeData().currentCombatTarget = it->targetHandle.get().get();
						break;
					}
					continue;
				}
			}
		}
		//a_actor->UpdateCombat();
	}


	bool OnMeleeHitHook::IsCasting(RE::Actor* a_actor)
	{
		bool result = false;

		if ((GetBoolVariable(a_actor, "IsCastingRight")) || (GetBoolVariable(a_actor, "IsCastingLeft")) || (GetBoolVariable(a_actor, "IsCastingDual")))
		{
			result = true;
		}

		return result;
	}

	void OnMeleeHitHook::InterruptAttack(RE::Actor* a_actor){
		a_actor->NotifyAnimationGraph("attackStop");
		a_actor->NotifyAnimationGraph("recoilStop");
		a_actor->NotifyAnimationGraph("bashStop");
		a_actor->NotifyAnimationGraph("blockStop");
		a_actor->NotifyAnimationGraph("staggerStop");
	}

	std::vector<RE::TESForm *> OnMeleeHitHook::GetEquippedForm(RE::Actor *actor, bool right, bool left)
	{
		std::vector<RE::TESForm *> Hen;

		auto limboform = actor->GetActorRuntimeData().currentProcess;

		if (right)
		{
			if (limboform && limboform->GetEquippedRightHand())
			{
				Hen.push_back(limboform->GetEquippedRightHand());
			}
		}
		else if (left)
		{
			if (limboform && limboform->GetEquippedLeftHand())
			{
				Hen.push_back(limboform->GetEquippedLeftHand());
			}
		}
		else
		{
			if (limboform && limboform->GetEquippedLeftHand())
			{
				Hen.push_back(limboform->GetEquippedLeftHand());
			}
			if (limboform && limboform->GetEquippedRightHand())
			{
				Hen.push_back(limboform->GetEquippedRightHand());
			}
		}
		return Hen;
	}

	int OnMeleeHitHook::GetEquippedItemType(RE::Actor *actor, bool lefthand)
	{
		using TYPE = RE::CombatInventoryItem::TYPE;
		int result = -1;
		auto form_list = lefthand ? GetEquippedForm(actor, false, true) : GetEquippedForm(actor, true);

		if (!form_list.empty())
		{
			for (auto form : form_list)
			{
				if (form)
				{
					switch (*form->formType)
					{
					case RE::FormType::Weapon:
						if (const auto equippedWeapon = form->As<RE::TESObjectWEAP>())
						{
							switch (equippedWeapon->GetWeaponType())
							{
							case RE::WEAPON_TYPE::kHandToHandMelee:
								result = 0;
								break;
							case RE::WEAPON_TYPE::kOneHandSword:
								result = 1;
								break;
							case RE::WEAPON_TYPE::kOneHandDagger:
								result = 2;
								break;
							case RE::WEAPON_TYPE::kOneHandAxe:
								result = 3;
								break;
							case RE::WEAPON_TYPE::kOneHandMace:
								result = 4;
								break;
							case RE::WEAPON_TYPE::kTwoHandSword:
								result = 5;
								break;
							case RE::WEAPON_TYPE::kTwoHandAxe:
								result = 6;
								break;
							case RE::WEAPON_TYPE::kBow:
								result = 7;
								break;
							case RE::WEAPON_TYPE::kStaff:
								result = 8;
								break;
							case RE::WEAPON_TYPE::kCrossbow:
								result = 12;
								break;
							default:
								break;
							}
						}
						break;

					default:
						break;
					}
					if (result != -1)
					{
						break;
					}
				}
				continue;
			}
		}

		if (result == -1)
		{
			auto combatCtrl = actor->GetActorRuntimeData().combatController;
			auto CombatInv = combatCtrl ? combatCtrl->inventory : nullptr;
			if (CombatInv)
			{
				for (const auto item : CombatInv->equippedItems)
				{
					if (item.item)
					{
						switch (item.item->GetType())
						{
						case TYPE::kTorch:
							result = 11;
							break;

						case TYPE::kShield:
							result = 10;
							break;

						case TYPE::kScroll:
						case TYPE::kMagic:
							result = 9;
							break;

						default:
							break;
						}
					}
					if (result != -1)
					{
						break;
					}
				}
			}
		}

		return result;
	}

	bool OnMeleeHitHook::IsRangedCombatant(RE::Actor *actor)
	{

		bool result = false;

		switch (GetEquippedItemType(actor, false))
		{
		case 7:
		case 8:
		case 9:
		case 12:
			result = true;
			break;

		default:

			break;
		}

		return result;
	}

	bool OnMeleeHitHook::IsHandToHandMelee(RE::Actor *actor)
	{
		bool left = false;
		bool right = false;

		switch (GetEquippedItemType(actor, false))
		{
		case 0:
			right = true;
			break;

		default:

			break;
		}

		switch (GetEquippedItemType(actor, true))
		{
		case 0:
			left = true;
			break;

		default:

			break;
		}

		return left && right;
	}

	bool OnMeleeHitHook::IsDualWieldMelee(RE::Actor *actor)
	{

		bool left = false;
		bool right = false;

		switch (GetEquippedItemType(actor, false))
		{
		case 1:
		case 2:
		case 3:
		case 4:
			right = true;
			break;

		default:

			break;
		}

		switch (GetEquippedItemType(actor, true))
		{
		case 1:
		case 2:
		case 3:
		case 4:
			left = true;
			break;

		default:

			break;
		}

		return left && right;
	}

	bool OnMeleeHitHook::IsWeaponOut(RE::Actor *actor)
	{
		bool result = false;
		auto form_list = GetEquippedForm(actor);

		if (!form_list.empty()) {
			for (auto form : form_list) {
				if (form) {
					switch (*form->formType) {
					case RE::FormType::Weapon:
						if (const auto equippedWeapon = form->As<RE::TESObjectWEAP>()) {
							switch (equippedWeapon->GetWeaponType()) {
							case RE::WEAPON_TYPE::kOneHandSword:
							case RE::WEAPON_TYPE::kOneHandDagger:
							case RE::WEAPON_TYPE::kOneHandAxe:
							case RE::WEAPON_TYPE::kOneHandMace:
							case RE::WEAPON_TYPE::kTwoHandSword:
							case RE::WEAPON_TYPE::kTwoHandAxe:
							case RE::WEAPON_TYPE::kBow:
							case RE::WEAPON_TYPE::kCrossbow:
								result = true;
								break;
							default:
								break;
							}
						}
						break;
					case RE::FormType::Armor:
						if (auto equippedShield = form->As<RE::TESObjectARMO>()) {
							result = true;
						}
						break;
					default:
						break;
					}
					if (result) {
						break;
					}
				}
				continue;
			}
		}
		return result;
	}

	bool OnMeleeHitHook::IsMeleeOnly(RE::Actor* a_actor)
	{
		using TYPE = RE::CombatInventoryItem::TYPE;

		auto result = false;

		auto combatCtrl = a_actor->GetActorRuntimeData().combatController;
		auto CombatInv = combatCtrl ? combatCtrl->inventory : nullptr;
		if (CombatInv) {
			for (const auto item : CombatInv->equippedItems) {
				if (item.item) {
					switch (item.item->GetType()) {
					case TYPE::kMelee:
						result = true;
						break;
					default:
						break;
					}
				}
				if (result){
					break;
				}
			}
		}

		return result;
	}

	bool OnMeleeHitHook::IsEquipped(RE::Actor *a_actor, RE::TESBoundObject * a_object)
	{
		bool result = false;

		if (auto combatCtrl = a_actor->GetActorRuntimeData().combatController; combatCtrl)
		{
			if (auto CombatInv = combatCtrl->inventory; CombatInv)
			{
				for (auto &invent_item : CombatInv->equippedItems)
				{
					if (invent_item.item && invent_item.item.get() && invent_item.item.get()->item && invent_item.item.get()->item == a_object)
					{
						result = true;

						break;
					}
				}
			}
		}
		return result;
	}

	void OnMeleeHitHook::EquipfromInvent(RE::Actor* a_actor, RE::FormID a_formID)
	{
		auto inv = a_actor->GetInventory();
		for (auto& [item, data] : inv) {
			const auto& [count, entry] = data;
			if (count > 0 && entry->object->formID == a_formID) {
				RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, entry->object);
				break;
			}
			continue;
		}
	}

	float OnMeleeHitHook::AV_Mod(RE::Actor* a_actor, int actor_value, float input, float mod)
	{
		if (actor_value > 0){
			int k;
			for (k = 0; k <= actor_value; k += 1) {
				input += mod;
			}
		}

		return input;
	}

	bool OnMeleeHitHook::GetBoolVariable(RE::Actor *a_actor, std::string a_string)
	{
		auto result = false;
		a_actor->GetGraphVariableBool(a_string, result);
		return result;
	}

	int OnMeleeHitHook::GetIntVariable(RE::Actor *a_actor, std::string a_string)
	{
		auto result = 0;
		a_actor->GetGraphVariableInt(a_string, result);
		return result;
	}

	float OnMeleeHitHook::GetFloatVariable(RE::Actor *a_actor, std::string a_string)
	{
		auto result = 0.0f;
		a_actor->GetGraphVariableFloat(a_string, result);
		return result;
	}


	class OurEventSink :
		public RE::BSTEventSink<RE::TESSwitchRaceCompleteEvent>,
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::TESCombatEvent>,
		public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
		public RE::BSTEventSink<RE::TESSpellCastEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>,
		public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
		OurEventSink() = default;
		OurEventSink(const OurEventSink&) = delete;
		OurEventSink(OurEventSink&&) = delete;
		OurEventSink& operator=(const OurEventSink&) = delete;
		OurEventSink& operator=(OurEventSink&&) = delete;

	public:
		static OurEventSink* GetSingleton()
		{
			static OurEventSink singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*)
		{
			auto a_actor = event->subject->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent *event, RE::BSTEventSource<RE::TESDeathEvent> *)
		{
			auto a_actor = event->actorDying->As<RE::Actor>();

			if (!a_actor)
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			if (a_actor->IsPlayerRef()){
				
			}else{
				
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*){

			if (!event || !event->actor)
			{
				return RE::BSEventNotifyControl::kContinue;
			}
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!(OnMeleeHitHook::GetBoolVariable(a_actor, "od") && OnMeleeHitHook::GetBoolVariable(a_actor, "dum")))
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(event->baseObject);
			if (!item)
			{
				return RE::BSEventNotifyControl::kContinue;
			}
			if (event->equipped)
			{
				if(item->IsMagicItem() && item->Is(RE::FormType::Spell)){

					// if (OnMeleeHitHook::GetBoolVariable(a_actor, "dum"))
					// {
					// 	a_actor->SetGraphVariableBool("dum", false);
					// 	OnMeleeHitHook::GetSingleton()->RegisterforUpdate(a_actor, std::make_tuple(nullptr, std::chrono::steady_clock::now(), 2000ms, "Dum_Update"));
					// }
				}
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
		{
			if (!event || event->eventName.empty())
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			if (event->eventName == "CustomYEvent")
			{
				logger::info("Custom Event Recieved");

				// Settings::GetSingleton()->Load();
			}


			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event, RE::BSTEventSource<RE::TESCombatEvent>*){
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor || a_actor->IsPlayerRef()) {
				return RE::BSEventNotifyControl::kContinue;
			}

			switch (event->newState.get()) {
			case RE::ACTOR_COMBAT_STATE::kCombat:

				break;

			case RE::ACTOR_COMBAT_STATE::kSearching:

				break;

			case RE::ACTOR_COMBAT_STATE::kNone:


				break;

			default:
				break;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*)
		{
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor || !a_actor->IsPlayerRef()) {
				return RE::BSEventNotifyControl::kContinue;
			}


			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
		{
			if (!event || !event->object)
			{
				return RE::BSEventNotifyControl::kContinue;
			}
			auto a_actor = event->object->As<RE::Actor>();

			if (!a_actor)
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!OnMeleeHitHook::GetBoolVariable(a_actor, "dya"))
			{
				return RE::BSEventNotifyControl::kContinue;
			}

			auto eSpell = RE::TESForm::LookupByID(event->spell);

			if (eSpell && eSpell->Is(RE::FormType::Spell)) {
				auto rSpell = eSpell->As<RE::SpellItem>();
				switch (rSpell->GetSpellType()) {
				case RE::MagicSystem::SpellType::kSpell:

					break;

				default:
					break;
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	bool DrinkPotionHook::Thunk(RE::Character *a_actor, RE::AlchemyItem *a_potion, RE::ExtraDataList *a_extralist)
	{
		// if (a_actor)
		// {
		// 	// logger::info("{} is requesting to drink potion", a_actor->GetName());

		// 	if (!(a_actor->Is3DLoaded() && a_actor->GetParentCell() && a_actor->GetParentCell()->cellState && a_actor->GetParentCell()->cellState == RE::TESObjectCELL::CellState::kAttached))
		// 	{
		// 		if (a_potion)
		// 		{
		// 			logger::info("Preventing {} from drinking {} in unloaded cell", a_actor->GetName(), a_potion->GetName());
		// 		}
		// 		else
		// 		{
		// 			logger::info("Preventing {} from drinking a potion in unloaded cell", a_actor->GetName());
		// 		}

		// 		return false;
		// 	}
		// }
		return _func(a_actor, a_potion, a_extralist);
	}

	void DrinkPotionHook::Install()
	{
		logger::info("Sinking drink potion hook for NPC");
		REL::Relocation<uintptr_t> npcPtr{RE::VTABLE_Character[0]};
		DrinkPotionHook::_func = npcPtr.write_vfunc(0x10F, DrinkPotionHook::Thunk);
		logger::info("Sinking complete.");
	}

	float OnMeleeHitHook::get_angle_he_me(RE::Actor *me, RE::Actor *he, RE::BGSAttackData *attackdata){
		auto he_me = PolarAngle(me->GetPosition() - he->GetPosition());
		auto head = PolarAngle(he->GetHeading(false) * 180.0f / PI);
		if (attackdata)
			head = head.add(attackdata->data.attackAngle);
		auto angle = he_me.sub(head).to_normangle();
		return angle;
	}

	RE::BGSAttackData* OnMeleeHitHook::get_attackData(RE::Actor *a){
		if (!a->GetActorRuntimeData().currentProcess || !a->GetActorRuntimeData().currentProcess->high)
			return nullptr;
		return a->GetActorRuntimeData().currentProcess->high->attackData.get();
	}

	void OnMeleeHitHook::getBodyPos(RE::Actor *a_actor, RE::NiPoint3 &pos)
	{
		if (!a_actor->GetActorRuntimeData().race) {
			return;
		}
		RE::BGSBodyPart* bodyPart = a_actor->GetActorRuntimeData().race->bodyPartData->parts[0];
		if (!bodyPart) {
			return;
		}
		auto targetPoint = a_actor->GetNodeByName(bodyPart->targetName.c_str());
		if (!targetPoint) {
			return;
		}

		pos = targetPoint->world.translate;
	}

	bool OnMeleeHitHook::PredictAimProjectile(RE::NiPoint3 a_projectilePos, RE::NiPoint3 a_targetPosition, RE::NiPoint3 a_targetVelocity, float a_gravity, RE::NiPoint3 &a_projectileVelocity)
	{
		// http://ringofblades.com/Blades/Code/PredictiveAim.cs

		float projectileSpeedSquared = a_projectileVelocity.SqrLength();
		float projectileSpeed = std::sqrtf(projectileSpeedSquared);

		if (projectileSpeed <= 0.f || a_projectilePos == a_targetPosition)
		{
			return false;
		}

		float targetSpeedSquared = a_targetVelocity.SqrLength();
		float targetSpeed = std::sqrtf(targetSpeedSquared);
		RE::NiPoint3 targetToProjectile = a_projectilePos - a_targetPosition;
		float distanceSquared = targetToProjectile.SqrLength();
		float distance = std::sqrtf(distanceSquared);
		RE::NiPoint3 direction = targetToProjectile;
		direction.Unitize();
		RE::NiPoint3 targetVelocityDirection = a_targetVelocity;
		targetVelocityDirection.Unitize();

		float cosTheta = (targetSpeedSquared > 0) ? direction.Dot(targetVelocityDirection) : 1.0f;

		bool bValidSolutionFound = true;
		float t;

		if (ApproximatelyEqual(projectileSpeedSquared, targetSpeedSquared))
		{
			// We want to avoid div/0 that can result from target and projectile traveling at the same speed
			// We know that cos(theta) of zero or less means there is no solution, since that would mean B goes backwards or leads to div/0 (infinity)
			if (cosTheta > 0)
			{
				t = 0.5f * distance / (targetSpeed * cosTheta);
			}
			else
			{
				bValidSolutionFound = false;
				t = 1;
			}
		}
		else
		{
			float a = projectileSpeedSquared - targetSpeedSquared;
			float b = 2.0f * distance * targetSpeed * cosTheta;
			float c = -distanceSquared;
			float discriminant = b * b - 4.0f * a * c;

			if (discriminant < 0)
			{
				// NaN
				bValidSolutionFound = false;
				t = 1;
			}
			else
			{
				// a will never be zero
				float uglyNumber = sqrtf(discriminant);
				float t0 = 0.5f * (-b + uglyNumber) / a;
				float t1 = 0.5f * (-b - uglyNumber) / a;

				// Assign the lowest positive time to t to aim at the earliest hit
				t = min(t0, t1);
				if (t < FLT_EPSILON)
				{
					t = max(t0, t1);
				}

				if (t < FLT_EPSILON)
				{
					// Time can't flow backwards when it comes to aiming.
					// No real solution was found, take a wild shot at the target's future location
					bValidSolutionFound = false;
					t = 1;
				}
			}
		}

		a_projectileVelocity = a_targetVelocity + (-targetToProjectile / t);

		if (!bValidSolutionFound)
		{
			a_projectileVelocity.Unitize();
			a_projectileVelocity *= projectileSpeed;
		}

		if (!ApproximatelyEqual(a_gravity, 0.f))
		{
			float netFallDistance = (a_projectileVelocity * t).z;
			float gravityCompensationSpeed = (netFallDistance + 0.5f * a_gravity * t * t) / t;
			a_projectileVelocity.z = gravityCompensationSpeed;
		}

		return bValidSolutionFound;
	}





	float OnMeleeHitHook::get_personal_survivalRatio(RE::Actor *protagonist, RE::Actor *combat_target)
	{
		float result = 0.0f;
		float MyTeam_total_threat = 0.0f;
		float EnemyTeam_total_threat = 0.0f;
		float personal_threat = 0.0f;

		if (const auto combatGroup = protagonist->GetCombatGroup())
		{
			for (auto &memberData : combatGroup->members)
			{
				if (auto ally = memberData.memberHandle.get(); ally)
				{
					if (auto value = memberData.threatValue; value)
					{
						MyTeam_total_threat += value;
						if (ally.get() == protagonist)
						{
							personal_threat += value;
						}
					}
				}
				continue;
			}
		}

		if (const auto combatGroup = combat_target->GetCombatGroup())
		{
			for (auto &memberData : combatGroup->members)
			{
				if (auto ally = memberData.memberHandle.get(); ally)
				{
					if (auto value = memberData.threatValue; value)
					{
						EnemyTeam_total_threat += value;
					}
				}
				continue;
			}
		}

		if (MyTeam_total_threat > 0 && EnemyTeam_total_threat > 0 && personal_threat > 0)
		{

			auto personal_survival = personal_threat / EnemyTeam_total_threat;
			auto Enemy_groupSurvival = EnemyTeam_total_threat / MyTeam_total_threat;

			result = personal_survival / Enemy_groupSurvival;
		}

		return result;
	}
	float OnMeleeHitHook::get_personal_threatRatio(RE::Actor *protagonist, RE::Actor *combat_target)
	{
		float result = 0.0f;
		float personal_threat = 0.0f;
		float CTarget_threat = 0.0f;

		if (const auto combatGroup = protagonist->GetCombatGroup())
		{
			for (auto &memberData : combatGroup->members)
			{
				if (auto ally = memberData.memberHandle.get(); ally)
				{
					if (ally.get() == protagonist)
					{
						if (auto value = memberData.threatValue; value)
						{
							personal_threat += value;
							break;
						}
					}
				}
				continue;
			}
		}

		if (const auto combatGroup = combat_target->GetCombatGroup())
		{
			for (auto &memberData : combatGroup->members)
			{
				if (auto ally = memberData.memberHandle.get(); ally)
				{
					if (ally.get() == combat_target)
					{
						if (auto value = memberData.threatValue; value)
						{
							CTarget_threat += value;
							break;
						}
					}
				}
				continue;
			}
		}

		if (personal_threat > 0 && CTarget_threat > 0)
		{
			result = personal_threat / CTarget_threat;
		}

		return result;
	}
	float OnMeleeHitHook::confidence_threshold(RE::Actor *a_actor, int confidence, bool inverse)
	{
		float result = 0.0f;

		if (inverse)
		{
			switch (confidence)
			{
			case 0:
				result = 0.1f;
				break;

			case 1:
				result = 0.3f;
				break;

			case 2:
				result = 0.5f;
				break;

			case 3:
				result = 0.7f;
				break;

			case 4:
				result = 0.9f;
				break;

			default:
				break;
			}
		}
		else
		{
			switch (confidence)
			{
			case 0:
				result = 1.25f;
				break;

			case 1:
				result = 1.0f;
				break;

			case 2:
				result = 0.75f;
				break;

			case 3:
				result = 0.5f;
				break;

			case 4:
				result = 0.25f;
				break;

			default:
				break;
			}
		}
		return result;
	}
	

	bool GetshouldHelp(const RE::Actor *p_ally, const RE::Actor *a_actor)
	{
		static RE::TESConditionItem cond;
		static std::once_flag flag;
		std::call_once(flag, [&]()
					   {
        cond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetShouldHelp;
        cond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        cond.data.comparisonValue.f     = 1.0f; });

		ConditionParam cond_param;
		cond_param.form = const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>());
		cond.data.functionData.params[0] = std::bit_cast<void *>(cond_param);

		RE::ConditionCheckParams params(const_cast<RE::TESObjectREFR *>(p_ally->As<RE::TESObjectREFR>()),
										const_cast<RE::TESObjectREFR *>(a_actor->As<RE::TESObjectREFR>()));
		return cond(params);
	}

	RE::BSEventNotifyControl animEventHandler::HookedProcessEvent(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src)
	{
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);

		if (!a_event.holder) {
			return fn ? (this->*fn)(a_event, src) : RE::BSEventNotifyControl::kContinue;
		}

		RE::Actor* actor = const_cast<RE::TESObjectREFR*>(a_event.holder)->As<RE::Actor>();
		switch (hash(a_event.tag.c_str(), a_event.tag.size())) {
		case "BeginCastLeft"_h:
		    
			break;

		default:
			break;
		}

		return fn ? (this->*fn)(a_event, src) : RE::BSEventNotifyControl::kContinue;
	}

	std::unordered_map<uint64_t, animEventHandler::FnProcessEvent> animEventHandler::fnHash;

	void OnMeleeHitHook::install(){

		auto eventSink = OurEventSink::GetSingleton();

		// ScriptSource
		auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		// eventSourceHolder->AddEventSink<RE::TESSwitchRaceCompleteEvent>(eventSink);
		//eventSourceHolder->AddEventSink<RE::TESEquipEvent>(eventSink);
		//eventSourceHolder->AddEventSink<RE::TESCombatEvent>(eventSink);
		//eventSourceHolder->AddEventSink<RE::TESActorLocationChangeEvent>(eventSink);
		//eventSourceHolder->AddEventSink<RE::TESSpellCastEvent>(eventSink);
		//eventSourceHolder->AddEventSink<RE::TESDeathEvent>(eventSink);
	}


	void OnMeleeHitHook::install_pluginListener(){
		auto eventSink = OurEventSink::GetSingleton();
		SKSE::GetModCallbackEventSource()->AddEventSink(eventSink);
	}

	InputEventHandler* InputEventHandler::GetSingleton()
	{
		static InputEventHandler singleton;
		return std::addressof(singleton);
	}

	RE::BSEventNotifyControl InputEventHandler::ProcessEvent(RE::InputEvent* const* a_event, [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
	{
		// using EventType = RE::INPUT_EVENT_TYPE;
		// using DeviceType = RE::INPUT_DEVICE;

		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// for (auto event = *a_event; event; event = event->next) {
		// 	if (event->eventType != EventType::kButton) {
		// 		continue;
		// 	}

		// 	auto button = static_cast<RE::ButtonEvent*>(event);
		// 	if (!button) {
		// 		continue;
		// 	}
		// 	auto key = button->idCode;
		// 	if (!key) {
		// 		continue;
		// 	}

		// 	switch (button->device.get()) {
		// 	case DeviceType::kMouse:
		// 		key += kMouseOffset;
		// 		break;
		// 	case DeviceType::kKeyboard:
		// 		key += kKeyboardOffset;
		// 		break;
		// 	case DeviceType::kGamepad:
		// 		key = GetGamepadIndex((RE::BSWin32GamepadDevice::Key)key);
		// 		break;
		// 	default:
		// 		continue;
		// 	}

		// 	if (key == 274)
		// 	{
		// 		auto Playerhandle = RE::PlayerCharacter::GetSingleton();
		// 		if (button->IsDown() || button->IsHeld() || button->IsPressed()){
		// 			Playerhandle->SetGraphVariableBool("bPSV_Toggle_PowerSprintAttack", true);
		// 		}
		// 		if(button->IsUp()){
		// 			Playerhandle->SetGraphVariableBool("bPSV_Toggle_PowerSprintAttack", false);
		// 		}
		// 		break;
		// 	}
		// }

		return RE::BSEventNotifyControl::kContinue;
	}

	std::uint32_t InputEventHandler::GetGamepadIndex(RE::BSWin32GamepadDevice::Key a_key)
	{
		using Key = RE::BSWin32GamepadDevice::Key;

		std::uint32_t index;
		switch (a_key) {
		case Key::kUp:
			index = 0;
			break;
		case Key::kDown:
			index = 1;
			break;
		case Key::kLeft:
			index = 2;
			break;
		case Key::kRight:
			index = 3;
			break;
		case Key::kStart:
			index = 4;
			break;
		case Key::kBack:
			index = 5;
			break;
		case Key::kLeftThumb:
			index = 6;
			break;
		case Key::kRightThumb:
			index = 7;
			break;
		case Key::kLeftShoulder:
			index = 8;
			break;
		case Key::kRightShoulder:
			index = 9;
			break;
		case Key::kA:
			index = 10;
			break;
		case Key::kB:
			index = 11;
			break;
		case Key::kX:
			index = 12;
			break;
		case Key::kY:
			index = 13;
			break;
		case Key::kLeftTrigger:
			index = 14;
			break;
		case Key::kRightTrigger:
			index = 15;
			break;
		default:
			index = kInvalid;
			break;
		}

		return index != kInvalid ? index + kGamepadOffset : kInvalid;
	}

	void InputEventHandler::SinkEventHandlers()
	{
		auto deviceManager = RE::BSInputDeviceManager::GetSingleton();
		deviceManager->AddEventSink(InputEventHandler::GetSingleton());
		logger::info("Added input event sink");
	}

	bool OnMeleeHitHook::BindPapyrusFunctions(VM* vm)
	{
		//vm->RegisterFunction("XXXX", "XXXXX", XXXX);
		return true;
	}

	int OnMeleeHitHook::GenerateRandomInt(int value_a, int value_b)
	{
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> dist(value_a, value_b);
		return dist(generator);
	}

	float OnMeleeHitHook::GenerateRandomFloat(float value_a, float value_b)
	{
		std::mt19937 generator(rd());
		std::uniform_real_distribution<float> dist(value_a, value_b);
		return dist(generator);
	}
	double OnMeleeHitHook::GenerateRandomDouble(double value_a, double value_b)
	{
		std::mt19937 generator(rd());
		std::uniform_real_distribution<double> dist(value_a, value_b);
		return dist(generator);
	}

	void OnMeleeHitHook::RegisterforUpdate(RE::Actor *a_actor, std::tuple<RE::MagicCaster *, std::chrono::steady_clock::time_point, std::chrono::milliseconds, std::string> data)
	{
		auto itt = _Timer.find(a_actor);
		if (itt == _Timer.end())
		{
			std::vector<std::tuple<RE::MagicCaster *, std::chrono::steady_clock::time_point, std::chrono::milliseconds, std::string>> Hen;
			Hen.push_back(data);
			_Timer.insert({a_actor, Hen});
		}
		else
		{
			itt->second.push_back(data);
		}
	}


	void OnMeleeHitHook::register_allied_target(RE::Actor *a_actor, RE::TESObjectREFR *a_ally)
	{
		auto itt = _AlliedTarget.find(a_actor);
		if (itt == _AlliedTarget.end())
		{
			_AlliedTarget.insert({a_actor, a_ally});
		}
		else
		{
			itt->second = a_ally;
		}
	}
	

	RE::TESObjectREFR *OnMeleeHitHook::get_allied_target(RE::Actor *a_actor)
	{
		RE::TESObjectREFR * a_ally = nullptr;
		for (auto it = _AlliedTarget.begin(); it != _AlliedTarget.end(); ++it)
		{
			if (it->first == a_actor)
			{
				if (it->second)
				{
					a_ally = it->second;
				}
				_AlliedTarget.erase(it);
				break;
			}
		}
		return a_ally;
	}

	void OnMeleeHitHook::clear_allied_targets([[maybe_unused]] RE::Actor *a_actor, bool clear_all)
	{
		if(!clear_all){
			auto itt = _AlliedTarget.find(a_actor);
			if (itt != _AlliedTarget.end())
			{
				_AlliedTarget.erase(itt);
			}
		}else{
			for (auto it = _AlliedTarget.begin(); it != _AlliedTarget.end(); ++it)
			{
				if (it->first)
				{
					_AlliedTarget.erase(it);
				}
			}
		}
	}

	void OnMeleeHitHook::Mod_CombatInventory_Claws(RE::Actor *a_actor, RE::CombatController *a_controller)
	{

		if (auto a_invent = a_controller->inventory; a_invent)
		{
			float score = 0;
			bool modded = false;
			int counter = 0;
			float highest_score = 0;
			RE::NiPointer<RE::CombatInventoryItem> highest_position = nullptr;
			RE::NiPointer<RE::CombatInventoryItem> position = nullptr;
			std::vector<RE::NiPointer<RE::CombatInventoryItem>> modded_store;

			for (auto invent_item : *a_invent->inventoryItems)
			{
				if (invent_item && invent_item.get() && invent_item.get()->item && invent_item.get()->itemScore 
				&& invent_item.get()->item->formID && invent_item.get()->item->formID == 0x1F4)
				{
					if (invent_item.get()->itemScore >= 500000)
					{
						if (!modded)
						{
							modded = true;
						}
						if (invent_item.get()->itemScore > highest_score)
						{
							highest_score = invent_item.get()->itemScore;
							highest_position = invent_item;
						}
						counter++;
						modded_store.push_back(invent_item);
					}
					else
					{
						if (invent_item.get()->itemScore > score)
						{
							score = invent_item.get()->itemScore;
							position = invent_item;
						}
					}
				}
			}

			if (modded)
			{
				if (counter > 1)
				{
					for (auto &a_modded_store : modded_store)
					{
						if (a_modded_store && a_modded_store.get() && highest_position && highest_position.get())
						{
							if (a_modded_store == highest_position)
							{
								continue;
							}
							else
							{
								a_modded_store.get()->itemScore -= 500000;
							}
						}
					}
				}
				else
				{
					return;
				}
			}
			else if (position && position.get() && score > 0)
			{
				if (!GetBoolVariable(a_actor, "bRBL_IsModdedClaws"))
				{
					a_actor->SetGraphVariableBool("bRBL_IsModdedClaws", true);
				}
				position.get()->itemScore += 500000;
			}
		}
	}

	void OnMeleeHitHook::Mod_CombatInventory_Claws_Reset(RE::Actor *a_actor, RE::CombatController *a_controller)
	{

		if (auto a_invent = a_controller->inventory; a_invent)
		{
			for (auto invent_item : *a_invent->inventoryItems)
			{
				if (invent_item && invent_item.get() && invent_item.get()->item && invent_item.get()->itemScore && invent_item.get()->item->formID 
				&& invent_item.get()->item->formID == 0x1F4)
				{
					if (invent_item.get()->itemScore >= 500000)
					{
						invent_item.get()->itemScore -= 500000;
					}
				}
			}
			a_actor->SetGraphVariableBool("bRBL_IsModdedClaws", false);
		}
	}

	void OnMeleeHitHook::Update(RE::Actor* a_actor, [[maybe_unused]] float a_delta)
	{
		if (a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->InHighProcess() && a_actor->Is3DLoaded() && a_actor->IsInCombat()){

			// GetSingleton()->Process_Updates(a_actor, std::chrono::steady_clock::now());

			// if (auto combatcontrol = a_actor->GetActorRuntimeData().combatController; combatcontrol)
			// {
			// 	if (getrace_IsWerebeast(a_actor))
			// 	{
			// 		Mod_CombatInventory_Claws(a_actor, combatcontrol);
			// 	}
			// 	else if (GetBoolVariable(a_actor, "bRBL_IsModdedClaws"))
			// 	{
			// 		Mod_CombatInventory_Claws_Reset(a_actor, combatcontrol);
			// 	}
			// }

			// RE::TESActionData a_data;
		}
	}

	bool AttackRangeCheck::CheckPathing(RE::Actor *enemy, RE::Actor *protagonist)
	{
		if (!enemy || !protagonist || !enemy->Get3D() || !protagonist->Get3D() || !protagonist->GetActorRuntimeData().currentProcess || !enemy->GetActorRuntimeData().currentProcess)
			return false;

		auto GetMeleeWeaponRange = [](RE::Actor *a_actor) -> float
		{
			using TYPE = RE::CombatInventoryItem::TYPE;
			float result = 0.f;
			if (a_actor)
			{
				auto combatCtrl = a_actor->GetActorRuntimeData().combatController;
				auto combatInv = combatCtrl ? combatCtrl->inventory : nullptr;
				if (combatInv)
				{
					for (const auto item : combatInv->equippedItems)
					{
						if (item.item && item.item->GetType() == TYPE::kMelee)
						{
							auto range = item.item->GetMaxRange();
							if (range > result)
								result = range;
						}
					}
				}
			}

			return result;
		};

		auto enemyPos = enemy->Get3D()->world.translate;
		auto protagonistPos = protagonist->Is3rdPersonVisible() ? protagonist->Get3D()->world.translate : protagonist->GetPosition();
		auto matrix = enemy->Get3D()->world.rotate;

		RE::NiMatrix3 invMatrix = matrix.Transpose();

		auto localVector = invMatrix * (protagonistPos - enemyPos);
		auto localDistance = std::sqrtf(localVector.x * localVector.x + localVector.y * localVector.y);
		if (localDistance <= GetMeleeWeaponRange(enemy) + GetBoundRadius(protagonist))
			return true;

		return CanNavigateToPosition(enemy, enemy->GetPosition(), protagonist->GetPosition(), 2.0f, GetBoundRadius(enemy));
	}

	bool AttackRangeCheck::WithinAttackRange(RE::Actor *enemy, RE::Actor *protagonist, float max_distance, float min_distance, float a_startAngle, float a_endAngle)
	{
		if (!enemy || !protagonist || !enemy->Get3D() || !protagonist->Get3D())
			return false;

		max_distance += GetBoundRadius(protagonist);
		if (min_distance > 0.f)
			min_distance += GetBoundRadius(protagonist) + GetBoundRadius(enemy);

		auto enemyPos = enemy->Get3D()->world.translate;
		auto protagonistPos = protagonist->Is3rdPersonVisible() ? protagonist->Get3D()->world.translate : protagonist->GetPosition();
		auto matrix = enemy->Get3D()->world.rotate;

		RE::NiMatrix3 invMatrix = matrix.Transpose();

		auto GetAabbTopBottomHeight = [](RE::Actor *a_actor, float &TopHeight, float &BottomHeight) -> bool
		{
			if (a_actor->Is3rdPersonVisible())
			{
				// ObjectBound bound;
				// if (Util::GetBBCharacter(a_actor, bound))
				// {
				// 	TopHeight = std::max(bound.worldBoundMin.z, bound.worldBoundMax.z);
				// 	BottomHeight = std::min(bound.worldBoundMin.z, bound.worldBoundMax.z);
				// 	return true;
				// }

				TopHeight = a_actor->GetPositionZ() + a_actor->GetBoundMax().z;
				BottomHeight = a_actor->GetPositionZ();
				return true;
			}
			else
			{
				TopHeight = a_actor->GetPositionZ() + a_actor->GetBoundMax().z;
				BottomHeight = a_actor->GetPositionZ();
				return true;
			}

			return false;
		};

		float enemyBottomHeight, enemyTopHeight, protagonistBottomHeight, protagonistTopHeight;
		if (!GetAabbTopBottomHeight(enemy, enemyTopHeight, enemyBottomHeight) || !GetAabbTopBottomHeight(protagonist, protagonistTopHeight, protagonistBottomHeight))
			return false;

		auto CheckHeight = [](const float enemyBottomHeight, const float enemyTopHeight, const float protagonistBottomHeight, const float protagonistTopHeight) -> bool
		{
			return (enemyTopHeight >= protagonistTopHeight && enemyBottomHeight <= protagonistTopHeight) || (enemyTopHeight >= protagonistBottomHeight && enemyBottomHeight <= protagonistBottomHeight) || (enemyTopHeight <= protagonistTopHeight && enemyBottomHeight >= protagonistBottomHeight);
		};

		if (!CheckHeight(enemyBottomHeight, enemyTopHeight, protagonistBottomHeight, protagonistTopHeight))
			return false;

		auto localVector = invMatrix * (protagonistPos - enemyPos);

		auto localDistance = std::sqrtf(localVector.x * localVector.x + localVector.y * localVector.y);
		auto localAngle = std::atan2f(localVector.x, localVector.y);
		if (localAngle < -std::numbers::pi)
			localAngle += 2 * std::numbers::pi;
		else if (localAngle > std::numbers::pi)
			localAngle -= 2 * std::numbers::pi;

		const bool withInAngle = a_startAngle < a_endAngle ? localAngle >= a_startAngle && localAngle <= a_endAngle : !(localAngle >= a_endAngle && localAngle <= a_startAngle);

		return localDistance <= max_distance && localDistance >= min_distance && withInAngle;
	}

	SCAR::DefaultObject SCAR::GetActionObject(std::string actionType)
	{
		static std::map<const std::string, const DefaultObject> actionMap = {
			{"RA", DefaultObject::kActionRightAttack},
			{"RPA", DefaultObject::kActionRightPowerAttack},
			{"LA", DefaultObject::kActionLeftAttack},
			{"LPA", DefaultObject::kActionLeftPowerAttack},
			{"DA", DefaultObject::kActionDualAttack},
			{"DPA", DefaultObject::kActionDualPowerAttack},
			{"BA", DefaultObject::kActionRightAttack},
			{"BPA", DefaultObject::kActionRightPowerAttack},
			{"IDLE", DefaultObject::kActionIdle}};

		auto itr = actionMap.find(actionType);
		return itr != actionMap.end() ? itr->second : DefaultObject::kActionRightAttack;
	}

	bool SCAR::PerformSCARAction(RE::Actor *protagonist, RE::Actor *enemy, bool unarmed)
	{
		if (!protagonist || !enemy || !protagonist->GetActorRuntimeData().currentProcess)
			return false;

		const float weaponReach = Actor_GetReach(enemy);
		if (AttackRangeCheck::WithinAttackRange(enemy, protagonist, 150.0f + weaponReach, 0.0f, -60.0f, 60.0f))
		{
			auto IdleAnimation = unarmed ? RE::TESForm::LookupByEditorID<RE::TESIdleForm>("NUB_H2H_Block_NPC") : RE::TESForm::LookupByEditorID<RE::TESIdleForm>("NUB_DW_Block_NPC");
			if (!IdleAnimation)
			{
				return false;
			}

			auto result = PlayIdle(protagonist->GetActorRuntimeData().currentProcess, protagonist, DefaultObject::kActionLeftAttack, IdleAnimation, true, true, enemy);

			if (result)
			{
				logger::info("{} successfully played block idle against {}", protagonist->GetName(), enemy->GetName());
			}else{

				logger::info("{} failed to play block idle against {}", protagonist->GetName(), enemy->GetName());
			}
				
			return result;
		}

		return false;
	}

	float SCAR::get_block_chance(RE::Actor *protagonist){
		float Score = 0.0f;

		/////////////////////////////////////////////////Defensive & Skirmish Weighting ///////////////////////////////////////////////////////////////////////////////////////////

		if (protagonist->GetActorRuntimeData().combatController)
		{
			RE::TESCombatStyle *style = protagonist->GetActorRuntimeData().combatController->combatStyle;
			if (style)
			{
				Score += style->generalData.defensiveMult * block_chance.Defensive_Weighting;
			}
		}

		/////////////////////////////////////////////////Sneak Skill Weighting /////////////////////////////////////////////////////////////////////////////////////////////////////////

		Score += (protagonist->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock) / 100.0f) * block_chance.Block_Weighting;

		return Score;
	}

	bool AttackActionHook::PerformAttackAction(RE::TESActionData *a_actionData)
	{
		auto protagonist = a_actionData && a_actionData->source ? a_actionData->source->As<RE::Actor>() : nullptr;
		auto enemy = protagonist ? protagonist->GetActorRuntimeData().currentCombatTarget.get() : nullptr;

		if (enemy && enemy.get() && GetLOS(protagonist, enemy.get()) && (enemy.get()->IsAttacking() || OnMeleeHitHook::GetBoolVariable(enemy.get(), "IsAttacking")) 
		&& OnMeleeHitHook::GetSingleton()->GenerateRandomFloat(0.0f, 1.0f) <= SCAR::GetSingleton()->get_block_chance(protagonist) && protagonist->GetActorRuntimeData().currentProcess 
		&& !protagonist->IsPlayerRef() && !OnMeleeHitHook::IsRangedCombatant(enemy.get()))
		{
			RE::BGSAttackData *attackdata = OnMeleeHitHook::GetSingleton()->get_attackData(enemy.get());
			auto angle = OnMeleeHitHook::GetSingleton()->get_angle_he_me(protagonist, enemy.get(), attackdata);

			float attackAngle = attackdata ? attackdata->data.strikeAngle : 35.0f;

			if (abs(angle) < attackAngle && AttackRangeCheck::CheckPathing(enemy.get(), protagonist))
			{
				
			}

			if (OnMeleeHitHook::IsHandToHandMelee(protagonist))
			{
				if (protagonist->HasKeywordString("ActorTypeNPC") && enemy.get()->HasKeywordString("ActorTypeNPC") 
				&& OnMeleeHitHook::isHumanoid(enemy.get()) && OnMeleeHitHook::isHumanoid(protagonist) && OnMeleeHitHook::IsHandToHandMelee(enemy.get()))
				{
					if (SCAR::GetSingleton()->PerformSCARAction(protagonist, enemy.get(), true))
					{
						return true;
					}
				}
			}
			else if (OnMeleeHitHook::IsDualWieldMelee(protagonist))
			{
				if (SCAR::GetSingleton()->PerformSCARAction(protagonist, enemy.get()))
				{
					return true;
				}
			}
		}

		return _PerformAttackAction(a_actionData);
	}

	void OnMeleeHitHook::Process_Updates(RE::Actor *a_actor, std::chrono::steady_clock::time_point time_now)
	{
		if (!a_actor)
		{
			return;
		}

		const auto &it = _Timer.find(a_actor);

		if (it != _Timer.end())
		{
			if (!it->second.empty())
			{
				for (auto data : it->second)
				{
					RE::MagicCaster *caster = nullptr;
					std::chrono::steady_clock::time_point time_initial;
					std::chrono::milliseconds time_required;
					std::string function;
					std::tie(caster, time_initial, time_required, function) = data;

					if (duration_cast<std::chrono::milliseconds>(time_now - time_initial).count() >= time_required.count())
					{
						auto H = RE::TESDataHandler::GetSingleton();
						switch (hash(function.c_str(), function.size()))
						{
						case "Dummy_Update"_h:

							break;

						default:
							break;
						}
						std::vector<std::tuple<RE::MagicCaster *, std::chrono::steady_clock::time_point, std::chrono::milliseconds, std::string>>::iterator position = std::find(it->second.begin(), it->second.end(), data);
						if (position != it->second.end())
						{
							it->second.erase(position);
						}
					}
				}
			}
			else
			{
				_Timer.erase(it);
			}
		}
	}

	void OnMeleeHitHook::init()
	{
		_precision_API = reinterpret_cast<PRECISION_API::IVPrecision1*>(PRECISION_API::RequestPluginAPI());
		if (_precision_API) {
			// _precision_API->AddPostHitCallback(SKSE::GetPluginHandle(), PrecisionWeaponsCallback_Post);
			logger::info("Enabled compatibility with Precision");
		}
	}

	// void OnMeleeHitHook::PrecisionWeaponsCallback_Post(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& a_hitdata)
	// {
	// 	if (!a_precisionHitData.target || !a_precisionHitData.target->Is(RE::FormType::ActorCharacter)) {
	// 		return;
	// 	}
	// 	return;
	// }

	void Settings::Load(){
		constexpr auto path = "Data\\SKSE\\Plugins\\Custom.ini";

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(path);

		general.Load(ini);

		include_spells_mods.Load(ini);
		include_spells_keywords.Load(ini);
		exclude_spells_mods.Load(ini);
		exclude_spells_keywords.Load(ini);

		ini.SaveFile(path);
	}

	

	void Settings::General_Settings::Load(CSimpleIniA &a_ini)
	{
		static const char *section = "General_Settings";

		auto DS = GetSingleton();

		DS->general.bWhiteListApproach = a_ini.GetBoolValue(section, "bWhiteListApproach", DS->general.bWhiteListApproach);
		DS->general.bPatchPureDamageSpells = a_ini.GetBoolValue(section, "bPatchPureDamageSpells", DS->general.bPatchPureDamageSpells);

		a_ini.SetBoolValue(section, "bWhiteListApproach", DS->general.bWhiteListApproach, ";If set to true, only the include mods and keywords are considered. Else only the exclude approach is used");
		a_ini.SetBoolValue(section, "bPatchPureDamageSpells", DS->general.bPatchPureDamageSpells, ";If set to false, pure NSV_Magic_Damage aren't patched which may help prevent strange mage behaviour if you experience them. However, Doing this will lead to less spell variety.");
	}

	void Settings::Include_AllSpells_withKeywords::Load(CSimpleIniA &a_ini)
	{
		static const char *section = "Include_AllSpells_withKeywords";

		auto DS = GetSingleton();

		DS->include_spells_keywords.inc_keywords_joined = a_ini.GetValue(section, "inc_keywords", DS->include_spells_keywords.inc_keywords_joined.c_str());

		std::istringstream f(DS->include_spells_keywords.inc_keywords_joined);
		std::string s;
		while (getline(f, s, '|'))
		{
			DS->include_spells_keywords.inc_keywords.push_back(s);
		}

		a_ini.SetValue(section, "inc_keywords", DS->include_spells_keywords.inc_keywords_joined.c_str(), ";Enter keywords for which all associated spells are included. Seperate keywords with | ");
	}

	void Settings::Include_AllSpells_inMods::Load(CSimpleIniA& a_ini){
		static const char* section = "Include_AllSpells_inMods";

		auto DS = GetSingleton();

		DS->include_spells_mods.inc_mods_joined = a_ini.GetValue(section, "inc_mods", DS->include_spells_mods.inc_mods_joined.c_str());

		std::istringstream f(DS->include_spells_mods.inc_mods_joined);
		std::string  s;
		while (getline(f, s, '|')) {
			DS->include_spells_mods.inc_mods.push_back(s);
		}

		a_ini.SetValue(section, "inc_mods", DS->include_spells_mods.inc_mods_joined.c_str(), ";Enter Mod Names of which all spells within are included. Seperate names with | ");
		//
	}

	void Settings::Exclude_AllSpells_withKeywords::Load(CSimpleIniA& a_ini)
	{
		static const char* section = "Exclude_AllSpells_withKeywords";

		auto DS = GetSingleton();

		DS->exclude_spells_keywords.exc_keywords_joined = a_ini.GetValue(section, "exc_keywords", DS->exclude_spells_keywords.exc_keywords_joined.c_str());

		std::istringstream f(DS->exclude_spells_keywords.exc_keywords_joined);
		std::string  s;
		while (getline(f, s, '|')) {
			DS->exclude_spells_keywords.exc_keywords.push_back(s);
		}

		a_ini.SetValue(section, "exc_keywords", DS->exclude_spells_keywords.exc_keywords_joined.c_str(), ";Enter keywords for which all associated spells are excluded. Seperate keywords with | ");
	}

	void Settings::Exclude_AllSpells_inMods::Load(CSimpleIniA &a_ini)
	{
		static const char *section = "Exclude_AllSpells_inMods";

		auto DS = GetSingleton();

		DS->exclude_spells_mods.exc_mods_joined = a_ini.GetValue(section, "exc_mods", DS->exclude_spells_mods.exc_mods_joined.c_str());

		std::istringstream f(DS->exclude_spells_mods.exc_mods_joined);
		std::string s;
		while (getline(f, s, '|'))
		{
			DS->exclude_spells_mods.exc_mods.push_back(s);
		}

		a_ini.SetValue(section, "exc_mods", DS->exclude_spells_mods.exc_mods_joined.c_str(), ";Enter Mod Names of which all spells within are excluded. Seperate names with | ");
		//
	}


}
