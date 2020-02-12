using System.Collections.Generic;
using System.Linq;
using DG.Tweening;
using NaughtyAttributes;
using UnityEngine;

public class TireStack : MonoBehaviour
{
    [InfoBox("The TireStack must be filled before runtime, and must be filled in bottom-top order", EInfoBoxType.Normal)]
    [Label("Tire Stack")]
    [SerializeField]
    [ReorderableList]
    private List<Tire> TireObjects;

    [SerializeField]
    private PlayerDetectionZone _DetectionZone;
    
    [SerializeField]
    [GameSavvy.OpenUnityAttributes.OnlyAssets(true)]
    private Tire _TirePrefab;

    [Header("Fancy effects tunables")]
    [SerializeField]
    private float SpawnEffectDuration = 0.1f;

    private readonly Queue<Tire> _CurrentTires = new Queue<Tire>();
    private Vector3[] _TirePositions;
    

    private void Awake()
    {
        _DetectionZone.EquipPressed.AddListener(EquipPressed);
        _TirePositions = new Vector3[TireObjects.Count];
        for (int i = 0; i < TireObjects.Count; i++)
        {
            _CurrentTires.Enqueue(TireObjects[i]);
            _TirePositions[i] = TireObjects[i].transform.position;
        }
        
    }

    private void EquipPressed(Player player)
    {
        if (_CurrentTires.Count <= 0)
            return;
        bool successfullyEqipped = player.ReceiveInteractable(_CurrentTires.Peek());
        if (successfullyEqipped)
        {
            var tire = _CurrentTires.Dequeue();
            tire.TireUsedEvent.AddListener(SpawnNewTire);
            MoveTiresDown();
        }
    }

    private void SpawnNewTire()
    {
        var newTire = (Tire)GameObject.Instantiate(_TirePrefab);
        _CurrentTires.Enqueue(newTire);
        newTire.transform.position = _TirePositions[_CurrentTires.Count - 1];
        Vector3 oldScale = newTire.transform.localScale;
        newTire.transform.localScale = Vector3.one * 0.01f;
        var tween = newTire.transform.DOScale(oldScale, SpawnEffectDuration);
        tween.SetEase(Ease.InOutElastic);

    }
    
    private void MoveTiresDown()
    {
        for (int i = 0; i < _CurrentTires.Count; i++)
        {
            _CurrentTires.ElementAt(i).transform.position = _TirePositions[i];
        }
    }
}
