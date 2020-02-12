using UnityEngine;
using UnityEngine.Events;
using FullService.Controls;
using NaughtyAttributes;

[System.Serializable]
public class PlayerUnityEvent : UnityEvent<Player>
{
}

public class Player : MonoBehaviour
{
    [SerializeField]
    private Transform _PlayerInteractableLocation;
    [SerializeField]
    private float _MovementSpeed = 3.0f;
    [SerializeField]
    private float _RotationSpeed = 3.0f;

    [SerializeField] private AnimationCurve _RotationCurve;

    private Rigidbody _Rigidbody;
    private Controls _InputActions;
    private Vector2 _MovementInput;

    public PlayerUnityEvent EquipPressed = new PlayerUnityEvent();
    public PlayerUnityEvent UsePressed = new PlayerUnityEvent();
    
    private IEquipable _CurrentEquipable;
    [ShowNativeProperty] public bool IsEquiped => _CurrentEquipable != null;

    private void Awake()
    {
        _InputActions = new Controls();
        _InputActions.PlayerControls.Move.performed += ctx => _MovementInput = ctx.ReadValue<Vector2>();
        _InputActions.PlayerControls.Equip.performed += ctx => EquipPressed.Invoke(this);
        _InputActions.PlayerControls.Use.performed += ctx => UsePressed.Invoke(this);
        _Rigidbody = GetComponent<Rigidbody>();
    }

    // This will get sent from an IPlayerInteractable
    public void ReceivePlayerInteraction(IPlayerInteraction interaction)
    {
        _CurrentEquipable?.Unequip(this, interaction);
    }

    /// <summary>
    /// This will get called from an object that hands out IEquipable, like for example, a tire stack
    /// </summary>
    /// <param name="equipable"></param>
    /// <returns>Returns wether or not the interactable was succesfully equipped</returns>
    public bool ReceiveInteractable(IEquipable equipable)
    {
        // We can only have one equipable
        if (_CurrentEquipable != null)
            return false;
        _CurrentEquipable = equipable;
        equipable.Equip(this);
        equipable.GameObject.transform.parent = _PlayerInteractableLocation;
        equipable.GameObject.transform.position = _PlayerInteractableLocation.position;
        return true;
    }
    
    public void DetachCurrentEquipable()
    {
        _CurrentEquipable = null;
    }

    private void OnEnable()
    {
        _InputActions.Enable();
    }

    private void OnDisable()
    {
        _InputActions.Disable();
    }

    private void FixedUpdate()
    {
        _Rigidbody.velocity = new Vector3(_MovementInput.x, 0, _MovementInput.y) * _MovementSpeed;
        CalculateRotation();
    }

    private void CalculateRotation()
    {
        if (_Rigidbody.velocity.magnitude > 0.4)
        {
            // Let's check if we're going backwards, because the dot product doesn't work as well if we're going directly backwards
            var angle = Vector3.Angle(-transform.forward.normalized, _Rigidbody.velocity.normalized);
            if (angle < 10 && angle > -10)
            {
                // If we're going backwards, let's just throw start some rotation
                _Rigidbody.angularVelocity = new Vector3(0, _RotationSpeed * 10, 0);
                return;
            }

            var dot = Vector3.Dot(transform.right.normalized, _Rigidbody.velocity.normalized);

            var torque = _RotationSpeed * new Vector3(0, _RotationCurve.Evaluate(dot), 0);

            _Rigidbody.angularVelocity = torque;
        }
        else
        {
            _Rigidbody.angularVelocity = Vector3.zero;
        }
    }

    private void LateUpdate()
    {
        // Just keep the rotation locked
        transform.localEulerAngles = new Vector3(0, transform.localEulerAngles.y, 0);
    }
    
}


public interface IPlayerInteraction
{
}

public class PlayerInteractionCar: IPlayerInteraction
{
    public PlayerInteractionCar(Car newCar)
    {
        car = newCar;
    }
    public Car car {get; private set;}
}
