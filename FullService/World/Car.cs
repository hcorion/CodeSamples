using System;
using UnityEngine;

public class Car : MonoBehaviour
{
    [SerializeField]
    private Tire[] _Wheels = new Tire[4];

    [SerializeField] private PlayerDetectionZone _PlayerZone;
	
    /// <summary>
    /// This should get called from a Player, that wants to equip the tire parameter onto the car
    /// </summary>
    /// <param name="player"></param>
    /// <param name="tire"></param>
    /// <returns> Return whether the tire was successfully equppied</returns>
    public bool EquipTire(Player player, Tire tire)
    {
        for (int i = 0; i < _Wheels.Length; i++)
        {
            if (_Wheels[i].IsUsed)
            {
                // This is a wheel we can replace
                var oldWheel = _Wheels[i];
                _Wheels[i] = tire;
                Vector3 oldScale = tire.transform.lossyScale;
                Transform oldWheelTransform = oldWheel.transform;
                Transform newTireTransform = _Wheels[i].transform;
                tire.transform.parent = oldWheel.transform.parent;
                newTireTransform.position = oldWheelTransform.position;
                newTireTransform.rotation = oldWheelTransform.rotation;
                newTireTransform.localScale = oldWheelTransform.localScale;
                
                Destroy(oldWheel.gameObject);
                return true;
            }
        }

        return false;
    }

    private void Awake()
    {
        _PlayerZone.EquipPressed.AddListener(PlayerPressedEquip);
    }

    private void PlayerPressedEquip(Player player)
    {
        player.ReceivePlayerInteraction(new PlayerInteractionCar(this));
    }
}
