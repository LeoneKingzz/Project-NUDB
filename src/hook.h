#include "PrecisionAPI.h"
#include "SKSE/Trampoline.h"
#include <SimpleIni.h>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <thread>
#include <numbers>
#include <MinHook.h>
#include "ClibUtil/editorID.hpp"
#pragma warning(disable: 4100)
#pragma warning(disable : 4189)
//using std::string;
static float& g_deltaTime = (*(float*)RELOCATION_ID(523660, 410199).address());


namespace hooks
{
	// static float& g_deltaTime = (*(float*)RELOCATION_ID(523660, 410199).address());
	using uniqueLocker = std::unique_lock<std::shared_mutex>;
	using sharedLocker = std::shared_lock<std::shared_mutex>;
	using VM = RE::BSScript::Internal::VirtualMachine;
	using StackID = RE::VMStackID;
#define STATIC_ARGS [[maybe_unused]] VM *a_vm, [[maybe_unused]] StackID a_stackID, RE::StaticFunctionTag *
#define PI 3.14159265358979323846f

	using EventResult = RE::BSEventNotifyControl;

	using tActor_IsMoving = bool (*)(RE::Actor* a_this);
	//static REL::Relocation<tActor_IsMoving> IsMoving{ REL::VariantID(36928, 37953, 0x6116C0) };

	typedef float (*tActor_GetReach)(RE::Actor* a_this);
	static REL::Relocation<tActor_GetReach> Actor_GetReach{ RELOCATION_ID(37588, 38538) };

	using tExtra_Quest = bool (*)(RE::ExtraDataList *a_this);
	static REL::Relocation<tExtra_Quest> HasQuestObjectAlias{RELOCATION_ID(11913, 12052)};

	union ConditionParam
	{
		char c;
		std::int32_t i;
		float f;
		RE::TESForm *form;
	};

	bool GetshouldHelp(const RE::Actor *p_ally, const RE::Actor *a_actor);
	bool Has_Magiceffect_Keyword(const RE::Actor *a_actor, const RE::BGSKeyword *a_key);
	bool HasBoundWeaponEquipped(const RE::Actor *a_actor, RE::MagicSystem::CastingSource type);
	bool GetLineOfSight(const RE::Actor *a_actor, const RE::Actor *a_target, float a_comparison_value);
	bool GetIsGhost(const RE::Actor *a_actor, float a_comparison_value);

	struct DrinkPotionHook
	{
	public:
		static bool Thunk(RE::Character *a_actor, RE::AlchemyItem *a_potion, RE::ExtraDataList *a_extralist);

		inline static REL::Relocation<decltype(&Thunk)> _func;

		static void Install();
	};



	template <class T>
	void copyComponent(RE::TESForm *from, RE::TESForm *to)
	{
		auto fromT = from->As<T>();
		auto toT = to->As<T>();
		if (fromT && toT)
		{
			toT->CopyComponent(fromT);
		}
	}

	class animEventHandler
	{
	private:
		template <class Ty>
		static Ty SafeWrite64Function(uintptr_t addr, Ty data)
		{
			DWORD oldProtect;
			void* _d[2];
			memcpy(_d, &data, sizeof(data));
			size_t len = sizeof(_d[0]);

			VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
			Ty olddata;
			memset(&olddata, 0, sizeof(Ty));
			memcpy(&olddata, (void*)addr, len);
			memcpy((void*)addr, &_d[0], len);
			VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
			return olddata;
		}

		typedef RE::BSEventNotifyControl (animEventHandler::*FnProcessEvent)(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* dispatcher);

		RE::BSEventNotifyControl HookedProcessEvent(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src);

		static void HookSink(uintptr_t ptr)
		{
			FnProcessEvent fn = SafeWrite64Function(ptr + 0x8, &animEventHandler::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(ptr, fn));
		}

	public:
		static animEventHandler* GetSingleton()
		{
			static animEventHandler singleton;
			return &singleton;
		}

		/*Hook anim event sink*/
		static void Register(bool player, bool NPC)
		{
			if (player) {
				logger::info("Sinking animation event hook for player");
				REL::Relocation<uintptr_t> pcPtr{ RE::VTABLE_PlayerCharacter[2] };
				HookSink(pcPtr.address());
			}
			if (NPC) {
				logger::info("Sinking animation event hook for NPC");
				REL::Relocation<uintptr_t> npcPtr{ RE::VTABLE_Character[2] };
				HookSink(npcPtr.address());
			}
			logger::info("Sinking complete.");
		}

		static void RegisterForPlayer()
		{
			Register(true, false);
		}

	protected:
		static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
	};

	class OnMeleeHitHook
	{
	public:

		static OnMeleeHitHook* GetSingleton()
		{
			static OnMeleeHitHook avInterface;
			return &avInterface;
		}

		static void install();
		static void install_pluginListener();

		static void install_protected(){
			Install_Update();
		}

		void init();

		static bool BindPapyrusFunctions(VM* vm);
		static void Set_iFrames(RE::Actor* actor);
		static void Reset_iFrames(RE::Actor* actor);
		static void dispelEffect(RE::MagicItem *spellForm, RE::Actor *a_actor);

		static void InterruptAttack(RE::Actor *a_actor);

		static void EquipfromInvent(RE::Actor *a_actor, RE::FormID a_formID);
		static bool IsEquipped(RE::Actor *a_actor, RE::TESBoundObject *a_object);

		static bool isPowerAttacking(RE::Actor *a_actor);
		static float GetActorValuePercent(RE::Actor *a_actor, RE::ActorValue a_value);
		static bool IsCasting(RE::Actor *a_actor);
		static void UpdateCombatTarget(RE::Actor* a_actor);
		static bool isHumanoid(RE::Actor *a_actor);
		static std::vector<RE::TESForm *> GetEquippedForm(RE::Actor *actor, bool right = false, bool left = false);
		static int GetEquippedItemType(RE::Actor *actor, bool lefthand);
		static bool IsRangedCombatant(RE::Actor *actor);
		static bool IsHandToHandMelee(RE::Actor *actor);
		static bool IsDualWieldMelee(RE::Actor *actor);
		static bool IsWeaponOut(RE::Actor* actor);
		void Update(RE::Actor* a_actor, float a_delta);
		float AV_Mod(RE::Actor *a_actor, int a_aggression, float input, float mod);
		int GenerateRandomInt(int value_a, int value_b);
	    float GenerateRandomFloat(float value_a, float value_b);
		double GenerateRandomDouble(double value_a, double value_b);
		static bool IsMeleeOnly(RE::Actor *a_actor);
		void UnequipAll(RE::Actor* a_actor);
		void Re_EquipAll(RE::Actor *a_actor);
		RE::BGSAttackData *get_attackData(RE::Actor *a);

		float confidence_threshold(RE::Actor *a_actor, int confidence, bool inverse = false);
		float get_personal_threatRatio(RE::Actor *protagonist, RE::Actor *combat_target);
		float get_personal_survivalRatio(RE::Actor *protagonist, RE::Actor *combat_target);
		void RegisterforUpdate(RE::Actor *a_actor, std::tuple<RE::MagicCaster *, std::chrono::steady_clock::time_point, std::chrono::milliseconds, std::string> data);
		void Process_Updates(RE::Actor *a_actor, std::chrono::steady_clock::time_point time_now);
		static bool GetBoolVariable(RE::Actor *a_actor, std::string a_string);
		static int GetIntVariable(RE::Actor *a_actor, std::string a_string);
		static float GetFloatVariable(RE::Actor *a_actor, std::string a_string);
		void register_allied_target(RE::Actor *a_actor, RE::TESObjectREFR *a_ally);
		RE::TESObjectREFR *get_allied_target(RE::Actor *a_actor);
		void clear_allied_targets(RE::Actor *a_actor, bool clear_all);
		static void Mod_CombatInventory_Claws(RE::Actor *a_actor, RE::CombatController *a_controller);
		static void Mod_CombatInventory_Claws_Reset(RE::Actor *a_actor, RE::CombatController *a_controller);
		static bool getrace_IsWerebeast(RE::Actor *a_actor)
		{
			bool result = false;
			const auto race = a_actor->GetRace();
			const auto raceEDID = race->formEditorID;
			if (raceEDID == "WerewolfBeastRace" || raceEDID == "DLC2WerebearBeastRace")
			{
				result = true;
			}
			return result;
		};

		static bool getrace_VLserana(RE::Actor *a_actor)
		{
			bool result = false;

			if (const auto race = a_actor->GetRace(); race)
			{
				const auto raceEDID = race->formEditorID;
				if (raceEDID == "DLC1VampireBeastRace")
				{
					if (a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))
					{
						result = true;
					}
				}
			}

			return result;
		};
		static bool Is_VLSserana(RE::Actor *a_actor)
		{
			bool result = false;

			if (a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))
			{
				result = true;
			}

			return result;
		};
		static void getBodyPos(RE::Actor *a_actor, RE::NiPoint3 &pos);
		static void InterruptCast(RE::MagicCaster *self, bool a_refund)
		{
			using func_t = decltype(&InterruptCast);
			REL::Relocation<func_t> func{RELOCATION_ID(33630, 34408)};
			return func(self, a_refund);
		};
		static bool PredictAimProjectile(RE::NiPoint3 a_projectilePos, RE::NiPoint3 a_targetPosition, RE::NiPoint3 a_targetVelocity, float a_gravity, RE::NiPoint3 &a_projectileVelocity);

		static inline bool ApproximatelyEqual(float A, float B)
		{
			return ((A - B) < FLT_EPSILON) && ((B - A) < FLT_EPSILON);
		}
		static inline void SetRotationMatrix(RE::NiMatrix3 &a_matrix, float sacb, float cacb, float sb)
		{
			float cb = std::sqrtf(1 - sb * sb);
			float ca = cacb / cb;
			float sa = sacb / cb;
			a_matrix.entry[0][0] = ca;
			a_matrix.entry[0][1] = -sacb;
			a_matrix.entry[0][2] = sa * sb;
			a_matrix.entry[1][0] = sa;
			a_matrix.entry[1][1] = cacb;
			a_matrix.entry[1][2] = -ca * sb;
			a_matrix.entry[2][0] = 0.0;
			a_matrix.entry[2][1] = sb;
			a_matrix.entry[2][2] = cb;
		}

		struct PolarAngle
		{
			float alpha;
			operator float() const { return alpha; }
			PolarAngle(float x = 0.0f) : alpha(x)
			{
				while (alpha > 360.0f)
					alpha -= 360.0f;
				while (alpha < 0.0f)
					alpha += 360.0f;
			}
			PolarAngle(const RE::NiPoint3 &p)
			{
				const float y = p.y;
				if (y == 0.0)
				{
					if (p.x <= 0.0)
						alpha = PI * 1.5f;
					else
						alpha = PI * 0.5f;
				}
				else
				{
					alpha = atanf(p.x / y);
					if (y < 0.0)
						alpha += PI;
				}
				alpha = alpha * 180.0f / PI;
			}
			PolarAngle add(const PolarAngle &r) const
			{
				float ans = alpha + r.alpha;
				if (ans >= 360.0f)
					return {ans - 360.0f};
				else
					return {ans};
			}
			PolarAngle sub(const PolarAngle &r) const
			{
				return this->add({360.0f - r.alpha});
			}
			float to_normangle() const
			{
				if (alpha > 180.0f)
					return alpha - 360.0f;
				else
					return alpha;
			}
			float to_normangle_abs() const
			{
				return abs(to_normangle());
			}
			static bool ordered(PolarAngle alpha, PolarAngle beta, PolarAngle gamma)
			{
				gamma = gamma.sub(alpha);
				beta = beta.sub(alpha);
				return gamma >= beta;
			}
			static float dist(float r, PolarAngle alpha)
			{
				auto ans = r * (alpha) / 180.0f * PI;
				return ans;
			}
		};

		float get_angle_he_me(RE::Actor *me, RE::Actor *he, RE::BGSAttackData *attackdata);

		template <class T>
		static std::vector<T*> get_all(const std::vector<RE::BGSKeyword*>& a_keywords)
		{
			std::vector<T*> result;

			if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
				for (const auto& form : dataHandler->GetFormArray<T>()) {
					if (!form || !a_keywords.empty() && !form->HasKeywordInArray(a_keywords, false)) {
						continue;
					}
					result.push_back(form);
				}
			}

			return result;
		}

		template <class T>
		static std::vector<T*> get_in_mod(const RE::TESFile* a_modInfo, const std::vector<RE::BGSKeyword*>& a_keywords)
		{
			std::vector<T*> result;

			if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
				for (const auto& form : dataHandler->GetFormArray<T>()) {
					if (!form || !a_modInfo->IsFormInMod(form->formID) || !a_keywords.empty() && !form->HasKeywordInArray(a_keywords, false)) {
						continue;
					}
					result.push_back(form);
				}
			}

			return result;
		}

		static std::vector<const RE::TESFile*> LookupMods(const std::vector<std::string>& modInfo_List)
		{
			std::vector<const RE::TESFile*> result;

			for (auto limbo_mod : modInfo_List) {
				if (!limbo_mod.empty()){
					const auto dataHandler = RE::TESDataHandler::GetSingleton();
					const auto modInfo = dataHandler ? dataHandler->LookupModByName(limbo_mod) : nullptr;

					if (modInfo){
						result.push_back(modInfo);
					}
				}
			}

			return result;
		}

		static std::vector<RE::BGSKeyword*> LookupKeywords(const std::vector<std::string>& keyword_List)
		{
			std::vector<RE::BGSKeyword*> result;

			for (auto limbo_key : keyword_List) {
				if (!limbo_key.empty()) {
					const auto key = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(limbo_key);

					if (key) {
						result.push_back(key);
					}
				}
			}

			return result;
		}

		template <class T>
		static std::vector<T*> get_valid_spellList(const std::vector<const RE::TESFile*>& exclude_modInfo_List, const std::vector<RE::BGSKeyword*>& exclude_keywords, bool whiteList_approach)
		{
			std::vector<T*> result;

			if (whiteList_approach){
				if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					for (const auto &form : dataHandler->GetFormArray<T>())
					{
						if (!form)
						{
							continue;
						}
						bool valid = false;
						for (const auto a_modInfo : exclude_modInfo_List)
						{
							if (a_modInfo && a_modInfo->IsFormInMod(form->formID))
							{
								valid = true;
								break;
							}
						}
						if (form->HasKeywordInArray(exclude_keywords, false))
						{
							valid = true;
						}
						if (valid)
						{
							result.push_back(form);
						}
					}
				}
			}else{

				if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					for (const auto &form : dataHandler->GetFormArray<T>())
					{
						if (!form)
						{
							continue;
						}
						bool invalid = false;
						for (const auto a_modInfo : exclude_modInfo_List)
						{
							if (a_modInfo && a_modInfo->IsFormInMod(form->formID))
							{
								invalid = true;
								break;
							}
						}
						if (form->HasKeywordInArray(exclude_keywords, false))
						{
							invalid = true;
						}
						if (!invalid)
						{
							result.push_back(form);
						}
					}
				}
			}

			return result;
		}

		PRECISION_API::IVPrecision1 *_precision_API = nullptr;

	private:
		OnMeleeHitHook() = default;
		OnMeleeHitHook(const OnMeleeHitHook&) = delete;
		OnMeleeHitHook(OnMeleeHitHook&&) = delete;
		~OnMeleeHitHook() = default;

		OnMeleeHitHook& operator=(const OnMeleeHitHook&) = delete;
		OnMeleeHitHook& operator=(OnMeleeHitHook&&) = delete;

		std::random_device rd;
		//static void PrecisionWeaponsCallback_Post(const PRECISION_API::PrecisionHitData& a_precisionHitData, const RE::HitData& a_hitdata);
		std::unordered_map<RE::Actor*, std::vector<RE::TESBoundObject*>> _Inventory;
		std::unordered_map<RE::Actor *, std::vector<RE::Projectile *>> _RunesCast;
		std::unordered_map<RE::Actor *, RE::TESObjectREFR *> _AlliedTarget;
		std::unordered_map<RE::Actor *, std::vector<std::pair<RE::TESBoundObject *, std::chrono::steady_clock::time_point>>> _Spells;

		std::shared_mutex mtx_RunesCast;
		std::shared_mutex mtx_Inventory;
		std::shared_mutex mtx_AlliedTarget;
		std::shared_mutex mtx__Spells;

	protected:

		struct Actor_Update
		{
			static void thunk(RE::Actor* a_actor, float a_delta)
			{
				func(a_actor, a_delta);
				GetSingleton()->Update(a_actor, g_deltaTime);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		

		static void Install_Update(){
			stl::write_vfunc<RE::Character, 0xAD, Actor_Update>();
		}
		std::unordered_map<RE::Actor *, std::vector<std::tuple<RE::MagicCaster *, std::chrono::steady_clock::time_point, std::chrono::milliseconds, std::string>>> _Timer;
	};

	class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:

		static InputEventHandler*	GetSingleton();

		virtual EventResult			ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

		static void SinkEventHandlers();

	private:
		enum : uint32_t
		{
			kInvalid = static_cast<uint32_t>(-1),
			kKeyboardOffset = 0,
			kMouseOffset = 256,
			kGamepadOffset = 266
		};

		InputEventHandler() = default;
		InputEventHandler(const InputEventHandler&) = delete;
		InputEventHandler(InputEventHandler&&) = delete;
		virtual ~InputEventHandler() = default;

		InputEventHandler& operator=(const InputEventHandler&) = delete;
		InputEventHandler& operator=(InputEventHandler&&) = delete;

		std::uint32_t GetGamepadIndex(RE::BSWin32GamepadDevice::Key a_key);
	};


	class util
	{
	private:
		static int soundHelper_a(void *manager, RE::BSSoundHandle *a2, int a3, int a4) // sub_140BEEE70
		{
			using func_t = decltype(&soundHelper_a);
			REL::Relocation<func_t> func{RELOCATION_ID(66401, 67663)};
			return func(manager, a2, a3, a4);
		}

		static void soundHelper_b(RE::BSSoundHandle *a1, RE::NiAVObject *source_node) // sub_140BEDB10
		{
			using func_t = decltype(&soundHelper_b);
			REL::Relocation<func_t> func{RELOCATION_ID(66375, 67636)};
			return func(a1, source_node);
		}

		static char __fastcall soundHelper_c(RE::BSSoundHandle *a1) // sub_140BED530
		{
			using func_t = decltype(&soundHelper_c);
			REL::Relocation<func_t> func{RELOCATION_ID(66355, 67616)};
			return func(a1);
		}

		static char set_sound_position(RE::BSSoundHandle *a1, float x, float y, float z)
		{
			using func_t = decltype(&set_sound_position);
			REL::Relocation<func_t> func{RELOCATION_ID(66370, 67631)};
			return func(a1, x, y, z);
		}

		std::random_device rd;

	public:
		static void playSound(RE::Actor *a, RE::BGSSoundDescriptorForm *a_descriptor)
		{
			//logger::info("starting voicing....");

			RE::BSSoundHandle handle;
			handle.soundID = static_cast<uint32_t>(-1);
			handle.assumeSuccess = false;
			*(uint32_t *)&handle.state = 0;

			soundHelper_a(RE::BSAudioManager::GetSingleton(), &handle, a_descriptor->GetFormID(), 16);

			if (set_sound_position(&handle, a->data.location.x, a->data.location.y, a->data.location.z))
			{
				soundHelper_b(&handle, a->Get3D());
				soundHelper_c(&handle);
				//logger::info("FormID {}"sv, a_descriptor->GetFormID());
				//logger::info("voicing complete");
			}
		}

		static RE::BGSSoundDescriptor *GetSoundRecord(const char* description)
		{

			auto Ygr = RE::TESForm::LookupByEditorID<RE::BGSSoundDescriptor>(description);

			return Ygr;
		}

		static util *GetSingleton()
		{
			static util singleton;
			return &singleton;
		}

		float GenerateRandomFloat(float value_a, float value_b)
		{
			std::mt19937 generator(rd());
			std::uniform_real_distribution<float> dist(value_a, value_b);
			return dist(generator);
		}
	};

	class Settings
	{
	public:
		static Settings* GetSingleton()
		{
			static Settings avInterface;
			return &avInterface;
		}

		void Load();

		struct General_Settings
		{
			void Load(CSimpleIniA &a_ini);
			bool bWhiteListApproach = false;
			bool bPatchPureDamageSpells = true;

		} general;

		struct Exclude_AllSpells_inMods
		{
			void Load(CSimpleIniA& a_ini);

			std::string exc_mods_joined = "Heroes of Yore.esp|VampireLordSeranaAssets.esp|VampireLordSerana.esp|TheBeastWithin.esp|TheBeastWithinHowls.esp";

			std::vector<std::string> exc_mods;

		} exclude_spells_mods;

		struct Exclude_AllSpells_withKeywords
		{
			void Load(CSimpleIniA& a_ini);
			std::string exc_keywords_joined = "HoY_MagicShoutSpell|LDP_MagicShoutSpell|NSV_CActorSpell_Exclude";

			std::vector<std::string> exc_keywords;

		} exclude_spells_keywords;

		struct Include_AllSpells_inMods
		{
			void Load(CSimpleIniA &a_ini);

			std::string inc_mods_joined = "Skyrim.esm|Dawnguard.esm|Dragonborn.esm";

			std::vector<std::string> inc_mods;

		} include_spells_mods;

		struct Include_AllSpells_withKeywords
		{
			void Load(CSimpleIniA &a_ini);
			std::string inc_keywords_joined = "DummyKey|ImposterKey";

			std::vector<std::string> inc_keywords;

		} include_spells_keywords;

	private:
		Settings() = default;
		Settings(const Settings&) = delete;
		Settings(Settings&&) = delete;
		~Settings() = default;

		Settings& operator=(const Settings&) = delete;
		Settings& operator=(Settings&&) = delete;
	};

	class AttackActionHook
	{
	public:
		static void InstallHook()
		{
			SKSE::AllocTrampoline(1 << 4);
			auto &trampoline = SKSE::GetTrampoline();

			REL::Relocation<std::uintptr_t> AttackActionBase{RELOCATION_ID(48139, 49170)};
			_PerformAttackAction = trampoline.write_call<5>(AttackActionBase.address() + REL::Relocate(0x4D7, 0x435), PerformAttackAction);
			// INFO("Hook PerformAttackAction!");
		};

		static bool GetLOS(RE::Actor *a_actor, RE::Actor *a_target)
		{
			auto result = false;
			return a_actor->HasLineOfSight(a_target, result) && result;
		};

	private:
		static bool PerformAttackAction(RE::TESActionData *a_actionData);

		static inline REL::Relocation<decltype(PerformAttackAction)> _PerformAttackAction;
	};

	class AttackRangeCheck
	{
	public:
		static bool CanNavigateToPosition(RE::Actor *a_actor, const RE::NiPoint3 &a_pos, const RE::NiPoint3 &a_new_pos, float a_speed, float a_distance)
		{
			using func_t = decltype(&CanNavigateToPosition);
			REL::Relocation<func_t> func{RELOCATION_ID(46050, 47314)};
			return func(a_actor, a_pos, a_new_pos, a_speed, a_distance);
		};

		static float GetBoundRadius(RE::Actor *a_actor)
		{
			using func_t = decltype(&GetBoundRadius);
			REL::Relocation<func_t> func{RELOCATION_ID(36444, 37439)};
			return func(a_actor);
		};

		static bool CheckPathing(RE::Actor *a_attacker, RE::Actor *a_target);

		static bool WithinAttackRange(RE::Actor *a_attacker, RE::Actor *a_targ, float max_distance, float min_distance, float a_startAngle, float a_endAngle);

		static void DrawOverlay(RE::Actor *a_attacker, RE::Actor *a_targ, float max_distance, float min_distance, float a_startAngle, float a_endAngle);
	};

	class SCAR
	{
	public:
		using DefaultObject = RE::BGSDefaultObjectManager::DefaultObject;

		static SCAR *GetSingleton()
		{
			static SCAR avInterface;
			return &avInterface;
		}

		bool PerformSCARAction(RE::Actor *a_attacker, RE::Actor *a_target);

		static bool PlayIdle(RE::AIProcess *a_this, RE::Actor *a_actor, RE::DEFAULT_OBJECT a_action, RE::TESIdleForm *a_idle, bool a_arg5, bool a_arg6, RE::TESObjectREFR *a_target)
		{
			using func_t = decltype(&PlayIdle);
			REL::Relocation<func_t> func{RELOCATION_ID(38290, 39256)};
			return func(a_this, a_actor, a_action, a_idle, a_arg5, a_arg6, a_target);
		};

		DefaultObject GetActionObject(std::string actionType);

		float get_block_chance(RE::Actor* protagonist);

		struct BlockChance_factors
		{
			float Block_Weighting = 0.5f;
			float Defensive_Weighting = 0.5f;

		} block_chance;

		// struct SCARActionData
		// {
		// private:
		// 	float weight = 0.f;

		// 	std::string IdleAnimationEditorID = "";

		// 	float minDistance = 0.f;

		// 	float maxDistance = 150.f;

		// 	float startAngle = -60.f;

		// 	float endAngle = 60.f;

		// 	float chance = 100.f;

		// 	std::string actionType = "RA";

		// 	std::optional<float> weaponLength;

		// public:

		// private:
		// 	const DefaultObject GetActionObject() const;
		// 	_NODISCARD const float GetStartAngle() const { return startAngle / 180.f * std::numbers::pi; };
		// 	_NODISCARD const float GetEndAngle() const { return endAngle / 180.f * std::numbers::pi; };

		// 	_NODISCARD const bool IsLeftAttack() const;
		// 	_NODISCARD const bool IsBashAttack() const;
		// 	_NODISCARD float GetWeaponReach(RE::Actor *a_attacker) const;
		// };

	private:

		SCAR() = default;
		SCAR(const SCAR &) = delete;
		SCAR(SCAR &&) = delete;
		~SCAR() = default;

		SCAR &operator=(const SCAR &) = delete;
		SCAR &operator=(SCAR &&) = delete;
	};
};

constexpr uint32_t hash(const char* data, size_t const size) noexcept
{
	uint32_t hash = 5381;

	for (const char* c = data; c < data + size; ++c) {
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	}

	return hash;
}

constexpr uint32_t operator"" _h(const char* str, size_t size) noexcept
{
	return hash(str, size);
}
