using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class GarbageBin : MonoBehaviour
{
    
    [SerializeField]
    private PlayerDetectionZone _DetectionZone;

    private void Awake()
    {
        _DetectionZone.EquipPressed.AddListener(EquipPressed);
    }

    private void EquipPressed(Player player)
    {
        player.ReceivePlayerInteraction(new PlayerInteractionGarbageBin());
    }
}

public class PlayerInteractionGarbageBin: IPlayerInteraction {}
