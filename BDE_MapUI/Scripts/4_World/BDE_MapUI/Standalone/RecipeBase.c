modded class RecipeBase
{
	protected static ref map<string, string> BDE_MapUI_m_SP_TypeDisplayNames;
	
	// Ajoute une fonction pour récupérer les ingrédients en regroupant les types d'objets similaires
	array<ref array<ref BDE_MapUI_CraftingItem>> BDE_MapUI_GetIngredients()
	{
		array<ref array<ref BDE_MapUI_CraftingItem>> allIngredients = new array<ref array<ref BDE_MapUI_CraftingItem>>();

		for (int i = 0; i < MAX_NUMBER_OF_INGREDIENTS; i++)
		{			
			map<string, ref BDE_MapUI_CraftingItem> tmp = new map<string, ref BDE_MapUI_CraftingItem>;
			map<string, ref array<string>> displayNamesTypes = new map<string, ref array<string>>;

			for (int j = 0; j < m_Ingredients[i].Count(); j++)
			{
				string className = m_Ingredients[i][j];
				
				// Vérification que l'objet existe
				if (!BDE_MapUI_ItemNames.IsItemExisting(className))
					continue;

				string displayName = "";

				bool updateAmount = true;

				if ( className.Contains("Pelt_Base")){
					className = "BearPelt";
				}

				// Récupère le nom et la quantité de l'ingrédient
				displayName = BDE_MapUI_ItemNames.GetItemDisplayNameWithType(className, BDE_MapUI_m_SP_TypeDisplayNames);

				//! Deal with the case where several different class names have the same display name
				array<string> displayNameTypes;
				if (displayNamesTypes.Find(displayName, displayNameTypes))
				{
					updateAmount = displayNameTypes.Find(className) > -1;
				}
				else
				{
					displayNameTypes = new array<string>;
					displayNamesTypes.Insert(displayName, displayNameTypes);
				}
				displayNameTypes.Insert(className);

				if (!displayName || displayName.IndexOf("$UNT$") == 0)  //! Skip not ready items
					continue;

				int amount = BDE_MapUI_GetAmount(className, m_MinQuantityIngredient[i]);

				BDE_MapUI_CraftingItem ingredient;
				
				if (tmp.Find(displayName, ingredient))
				{
					if (updateAmount)
						ingredient.amount += amount;  // Ajoute la quantité si l'ingrédient est déjà présent
				}
				else
				{
					ingredient = new BDE_MapUI_CraftingItem(displayName, amount);
					tmp.Insert(displayName, ingredient);
				}
				ingredient.classNames.Insert(className);  // Ajouter le nom de la classe à l'ingrédient
			}

			allIngredients.Insert(BDE_MapUI_GetItems(tmp));
		}

		return allIngredients;
	}

	array<ref BDE_MapUI_CraftingItem> BDE_MapUI_GetResults()
	{
		map<string, ref BDE_MapUI_CraftingItem> tmp = new map<string, ref BDE_MapUI_CraftingItem>;
		for (int i = 0; i < m_NumberOfResults; i++)
		{
			string className = m_ItemsToCreate[i];
			string displayName = "";

			if (m_ResultInheritsColor[i] != -1)
				className += "ColorBase";  // Modifie le nom de la classe si nécessaire

			if ( className.Contains("Pelt_Base")){
				className = "BearPelt";
			}

			displayName = BDE_MapUI_ItemNames.GetItemDisplayNameWithType(className, BDE_MapUI_m_SP_TypeDisplayNames);

			if (!displayName || displayName.IndexOf("$UNT$") == 0)  // Skip not ready items
				continue;

			int amount = BDE_MapUI_GetAmount(className, m_ResultSetQuantity[i]);

			BDE_MapUI_CraftingItem result;
			if (tmp.Find(displayName, result))
			{
				result.amount += amount;
			}
			else
			{
				result = new BDE_MapUI_CraftingItem(displayName, amount);
				result.classNames.Insert(className);  // Ajouter le nom de la classe ici
				tmp.Insert(displayName, result);
			}
		}

		return BDE_MapUI_GetItems(tmp);
	}

	// Helper function pour obtenir les items à partir du map
	array<ref BDE_MapUI_CraftingItem> BDE_MapUI_GetItems(map<string, ref BDE_MapUI_CraftingItem> tmp)
	{
		array<ref BDE_MapUI_CraftingItem> items = new array<ref BDE_MapUI_CraftingItem>();
		foreach (string displayName, BDE_MapUI_CraftingItem item: tmp)
		{
			items.Insert(item);
		}
		return items;
	}

	// Obtient la quantité d'un ingrédient en fonction de sa classe et quantité
	int BDE_MapUI_GetAmount(string className, int quantity)
	{
		if (quantity != -1)
		{
			TStringArray cfgSearches = {CFG_VEHICLESPATH, CFG_MAGAZINESPATH};
			foreach (string cfgSearch : cfgSearches)
			{
				string cfgPath = cfgSearch + " " + className;
				if (g_Game.ConfigGetInt(cfgPath + " canBeSplit") && !g_Game.ConfigGetInt(cfgPath + " quantityBar"))
				{
					return quantity;
				}
			}
		}

		return 1;
	}

	bool BDE_MapUI_CanBeShownInCraftingTab(){
		return true;
	}
}
