using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Assertions;
using UnityEngine.Events;

public class Tire : MonoBehaviour, IEquipable
{
    /* If the tire is in a used state, then we can't put it on a new car, */
    [SerializeField] private bool _IsUsed = false;

    public bool IsUsed => _IsUsed;

    [SerializeField]
    private Collider _Collision;

    static readonly OnCar State_OnCar = new OnCar();
    static readonly OnTireStack State_OnTireStack = new OnTireStack();
    static readonly PlayerHas State_PlayerHas = new PlayerHas();

    public UnityEvent TireUsedEvent;

    /*This should only be modified via the EquipableState.ToState*/
    public IEquipableState _DoNotModifyState;
    /*This should only be modified via the EquipableState.ToState*/
    public IEquipableState State { get => _DoNotModifyState; set => _DoNotModifyState = value; }

    public GameObject GameObject => this.gameObject;

    public void Awake()
    {
        // We assume that the Tire is on the tire stack by default
        State = new OnTireStack();
        if (_Collision == null)
        {
            _Collision = GetComponentInChildren<Collider>();
        }
    }

    public void Equip(Player player)
    {
        State.ToState(this, State_PlayerHas);
    }

    public void Unequip(Player player, IPlayerInteraction playerInteraction)
    {
        var carInteraction = playerInteraction as PlayerInteractionCar;
        var garbageInteraction = playerInteraction as PlayerInteractionGarbageBin;
        if (carInteraction != null)
        {
            // Looks like we're trying to unequip the current tire onto a car
            bool successfullyEquipped = carInteraction.car.EquipTire(player, this);
            if (successfullyEquipped)
            {
                State.ToState(this, State_OnCar);
                player.DetachCurrentEquipable();
            }
            return;
        }
        else if (garbageInteraction != null)
        {
            // The play just wants to throw the tire in the garbage
            player.DetachCurrentEquipable();
            TireUsedEvent.Invoke();
            Destroy(gameObject);
        }
    }

    public void Use(Player player, IPlayerInteraction playerInteraction)
    {
        // You can't really "use" a tire.
    }

    void Update()
    {
        if (State != null)
        {
            State.Update(this);
        }
    }

    public class OnCar : EquipableStateBase
    {
        public override void OnEnter(IEquipable equipable)
        {
            var tire = (Tire)equipable;
            Assert.IsNotNull(tire);
            tire.TireUsedEvent.Invoke();
            tire._Collision.enabled = false;
        }
    }

    public class PlayerHas : EquipableStateBase
    {
        public override void OnEnter(IEquipable equipable)
        {
            var tire = (Tire)equipable;
            Assert.IsNotNull(tire);
            tire._Collision.enabled = false;
        }
    }

    public class OnTireStack : EquipableStateBase {}
}