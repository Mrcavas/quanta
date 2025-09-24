import { Accessor, createSignal, onMount } from "solid-js"

export function createSessionSignal<T>(
  name: string,
  invalidationTime: number
): [Accessor<T | undefined>, (v?: T) => void]
export function createSessionSignal<T>(
  name: string,
  invalidationTime: number,
  initial: T,
  storage?: Storage
): [Accessor<T>, (v?: T) => void]
export function createSessionSignal<T>(
  name: string,
  invalidationTime: number,
  initial?: T,
  storage: Storage = sessionStorage
): [Accessor<T | undefined>, (v?: T) => void] {
  const [value, setValue] = createSignal(initial)

  onMount(() => {
    const stored = storage.getItem(name)
    if (stored) {
      const parsed = JSON.parse(stored)
      if ("invalidateAfter" in parsed && Date.now() > parsed.invalidateAfter) {
        storage.removeItem(name)
        setValue(initial as Exclude<T, Function>)
      } else setValue(parsed.value)
    }
  })

  return [
    value,
    v => {
      if (v === undefined) storage.removeItem(name)
      else {
        if (invalidationTime === -1)
          storage.setItem(
            name,
            JSON.stringify({
              value: v,
            })
          )
        else
          storage.setItem(
            name,
            JSON.stringify({
              value: v,
              invalidateAfter: Date.now() + invalidationTime,
            })
          )
      }
      setValue((v ?? initial) as Exclude<T, Function>)
    },
  ]
}
