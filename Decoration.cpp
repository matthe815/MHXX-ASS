#include "stdafx.h"
#include "Decoration.h"
#include "Skill.h"
#include "Solution.h"

using namespace System;

void Decoration::Load( System::String^ filename )
{
	static_decoration_map.Clear();
	static_decoration_ability_map.Clear();
	static_decorations.Clear();
	static_decorations.Capacity = 256;
	IO::StreamReader fin( filename );
	while( !fin.EndOfStream )
	{
		String^ line = fin.ReadLine();
		if( line == L"" || line[ 0 ] == L'#' )
			continue;
		List_t< String^ > split;
		Utility::SplitString( %split, line, L',' );
		Decoration^ decoration = gcnew Decoration;
		decoration->is_event = false;
		//name,rarity,slots,hr,ve,mode,skill1,#,skill2,#,mat1,#,mat2,#,mat3,#,mat4,#
		decoration->name = split[ 0 ];
		decoration->jap_name = split[ 0 ];
		const unsigned data_start = 1;
		decoration->rarity = Convert::ToUInt32( split[ data_start ] );
		decoration->slots_required = Convert::ToUInt32( split[ data_start + 1 ] );
		decoration->hr = Convert::ToUInt32( split[ data_start + 2 ] );
		decoration->elder_star = Convert::ToUInt32( split[ data_start + 3 ] );
		decoration->difficulty = 0;

		//mode: split[ data_start + 4 ] 0=or 1=and
		decoration->hr_or = !Convert::ToUInt32( split[ data_start + 4 ] );

		for( int i = 0; i < 2; ++i )
		{
			if( split[ data_start + 5 + i * 2 ] == L"" )
				continue;

			String^ ab = split[ data_start + 5 + i * 2 ];
			AbilityPair^ apair = gcnew AbilityPair( Ability::FindAbility( ab ), Convert::ToInt32( split[ data_start + 6 + i * 2 ] ) );
			Assert( apair->ability, L"No such ability found" );
			decoration->abilities.Add( apair );
			if( apair->amount > 0 )
			{
				apair->ability->has_decorations = true;
				if( decoration->slots_required == 1 )
					apair->ability->has_1slot_decorations = true;
			}
		}
		for( unsigned i = 0; i < 4; ++i )
		{
			String^% name = split[ data_start + 9 + i * 2 ];
			if( name != L"" )
			{
				MaterialComponent^ mc = gcnew MaterialComponent;
				mc->amount = Convert::ToUInt32( split[ data_start + 10 + i * 2 ] );
				mc->material = Material::FindMaterial( name );
				if( mc->material )
				{
					decoration->components.Add( mc );
					decoration->is_event |= mc->material->event_only;
					decoration->difficulty += mc->material->difficulty;
				}
			}
			if( split[ data_start + 17 + i * 2 ] != L"" && !split[ data_start + 7 + i * 2 ]->StartsWith( L"※" ) )
			{
				MaterialComponent^ mc = gcnew MaterialComponent;
				mc->amount = Convert::ToUInt32( split[ data_start + 18 + i * 2 ] );
				mc->material = Material::FindMaterial( split[ data_start + 17 + i * 2 ] );
				if( mc->material )
					decoration->components2.Add( mc );
			}
			//decoration->ping_index = Convert::ToUInt32( split[ data_start + 24 ] );
		}

		decoration->index = static_decorations.Count;
		static_decorations.Add( decoration );
		if( !static_decoration_ability_map.ContainsKey( decoration->abilities[ 0 ]->ability ) )
			static_decoration_ability_map.Add( decoration->abilities[ 0 ]->ability, gcnew List_t< Decoration^ > );
		static_decoration_ability_map[ decoration->abilities[ 0 ]->ability ]->Add( decoration );
		static_decoration_map.Add( decoration->name, decoration );
	}
	static_decorations.TrimExcess();
}

int Decoration::GetSkillAt( Ability^ ability )
{
	for( int i = 0; i < abilities.Count; ++i )
		if( abilities[ i ]->ability == ability )
			return abilities[ i ]->amount;
	return 0;
}

inline bool NotWorse( Decoration^ a, Decoration^ b )
{
	return b->abilities.Count == 2 && ( a->abilities.Count == 1 || a->abilities[ 1 ]->amount * b->slots_required < b->abilities[ 1 ]->amount * a->slots_required );
}

bool Decoration::IsBetterThan( Decoration^ other, List_t< Ability^ >^ rel_abilities )
{
	if( slots_required < other->slots_required || abilities[ 0 ]->ability != other->abilities[ 0 ]->ability )
		return true;
	const int a = abilities[ 0 ]->amount * other->slots_required,
			  b = other->abilities[ 0 ]->amount * slots_required;
	return a != b ? a > b : NotWorse( this, other );
}

Decoration^ Decoration::GetBestDecoration( Ability^ ability, const unsigned max_slots, const unsigned hr, const unsigned elder_star )
{
	//Assert( !ability->relevant, L"Ability is relevant here!?" );

	if( !static_decoration_ability_map.ContainsKey( ability ) )
		return nullptr; //no decorations exist for this skill

	Decoration^ best = nullptr;
	List_t< Ability^ > rel;
	rel.Add( ability );
	for each( Decoration^ dec in static_decoration_ability_map[ ability ] )
	{
		if( !Query::HRMatches( dec, hr, elder_star )|| dec->slots_required > max_slots )
			continue;
		for each( AbilityPair^ ap in dec->abilities )
		{
			if( ap->amount > 0 && ap->ability == ability )
			{
				if( !best || dec->IsBetterThan( best, %rel ) )
					best = dec;
			}
		}
	}
	return best;
}

bool Decoration::MatchesQuery( Query^ query )
{
	//check requirements
	if( !Query::HRMatches( this, query->hr, query->elder_star ) ||
		is_event && !query->allow_event )
		return false;

	//check for relevant skills
	for each( Skill^ skill in query->skills )
	{
		if( skill->ability == abilities[ 0 ]->ability )
			return true;
	}
	return false;
}

Decoration^ Decoration::FindDecoration( System::String^ name )
{
	if( static_decoration_map.ContainsKey( name ) )
		return static_decoration_map[ name ];
	return nullptr;
}

Decoration^ Decoration::FindDecorationFromString( System::String^ line )
{
	//1x Expert +5 Jewel
	const int plus = line->IndexOf( L'+' );
	const unsigned points = Convert::ToUInt32( line->Substring( plus + 1, 1 ) );

	Ability^ best_ability = nullptr;
	for each( Ability^ ability in Ability::static_abilities )
	{
		if( line->IndexOf( ability->name ) > -1 )
		{
			if( !best_ability || best_ability->name->Length < ability->name->Length )
				best_ability = ability;
		}
	}

	if( !best_ability )
		return nullptr;

	for each( Decoration^ dec in static_decorations )
	{
		if( dec->abilities[ 0 ]->ability == best_ability &&
			dec->abilities[ 0 ]->amount == points )
			return dec;
	}
	return nullptr;
}

Decoration^ Decoration::GetBestDecoration( Ability^ ability, const unsigned max_slots, List_t< List_t< Decoration^ >^ >% rel_deco_map )
{
	for( int i = max_slots + 1; i --> 0; )
	{
		for each( Decoration^ deco in rel_deco_map[ i ] )
		{
			if( deco->abilities[ 0 ]->ability == ability )
				return deco;
		}
	}
	return nullptr;
}

void Decoration::LoadLanguage( System::String^ filename )
{
	static_decoration_map.Clear();
	IO::StreamReader fin( filename );
	for( int i = 0; i < static_decorations.Count; )
	{
		String^ line = fin.ReadLine();
		if( line == L"" || line[ 0 ] == L'#' )
			continue;
		static_decorations[ i ]->name = line;
		if( !StringTable::japanese )
			static_decorations[ i ]->eng_name = line;
		static_decoration_map.Add( line, static_decorations[ i ] );
		i++;
	}
}

void Decoration::RecheckEfficientDecorations( Query^ query )
{
	for each( Ability^ ab in Ability::static_abilities )
	{
		ab->efficient = false;
	}

	for each( Decoration^ deco in static_decorations )
	{
		if( deco->hr > query->hr && deco->elder_star > query->elder_star ||
			deco->is_event && !query->allow_event )
			continue;

		if( deco->slots_required == 1 && deco->abilities.Count == 1 && deco->abilities[0]->amount == 2 )
			deco->abilities[0]->ability->efficient = true;
	}
}
