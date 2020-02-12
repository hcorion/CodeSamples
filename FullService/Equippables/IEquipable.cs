public interface IEquipable
{
    IEquipableState State{get; set;}
    UnityEngine.GameObject GameObject{get;}
    void Equip(Player player);
    void Unequip(Player player, IPlayerInteraction playerInteraction);
    void Use(Player player, IPlayerInteraction playerInteraction);
}

public interface IEquipableState
{
    void OnEnter(IEquipable equipable);
    void OnExit(IEquipable equipable);
    void Update(IEquipable equipable);
    void ToState(IEquipable equipable, IEquipableState newState);
}

public abstract class EquipableStateBase: IEquipableState
{
    public virtual void OnEnter(IEquipable equipable) {}
    public virtual void OnExit(IEquipable equipable) {}
    public virtual void Update(IEquipable equipable) {}
    public virtual void ToState(IEquipable equipable, IEquipableState newState)
    {
        equipable.State.OnExit(equipable);
        equipable.State = newState;
        equipable.State.OnEnter(equipable);
    }
}