-- Utilities
--------------------------------------------------------

fmt = string.format

function string:ends(v)
  return self:sub(0 - #v) == v
  --return string.find(self, v, #self-#v, true) ~= nil
end

function string:split(sep)
 local sep = sep or ":"
 local fields = {}
 local pattern = string.format("([^%s]+)", sep)
 self:gsub(pattern, function(c) fields[#fields+1] = c end)
 if self:sub(0, 1) == sep then table.insert(fields, 1, "") end
 if self:sub(-1) == sep then table.insert(fields, "") end
 if #fields == 0 then fields[1] = "" end
 return fields
end

function string:filename()
  local parts = self:split("/")
  return parts[#parts]
end

function padl(value, len, padding)
  value = tostring(value)
  while #value < len do
    value = padding .. value
  end
  return value
end

function pformat(v, x, y, z)
  local b = ""
  local t = type(v)
  if t == "nil" then
    b = b .. "nil"
  elseif t == "number" or t == "boolean" then
    b = b .. tostring(v)
  elseif t == "string" then
    b = b .. "\"" .. tostring(v) .. "\""
  elseif t == "function" or t == "thread" or t == "userdata" then
    b = b .. tostring(v)
  elseif t == "table" then
    b = b .. "{"
    for k, vv in pairs(v) do
      b = b .. k .. "=" .. pformat(vv) .. ","
    end
    b = b .. "}"
  end
  if x ~= nil then b = b .. " " .. pformat(x) end
  if y ~= nil then b = b .. " " .. pformat(y) end
  if z ~= nil then b = b .. " " .. pformat(z) end
  return b
end

function map(list, fn)
  local result = {}
  for i, v in ipairs(list) do
    local value = fn(v, i)
    table.insert(result, value)
  end
  return result
end

function log(f, ...)
  local message = string.format(f.."\n", ...)
  io.stderr:write(message)
end


-- Editor
--------------------------------------------------------

editor = {
  width = 0,
  height = 0,
  keys = "",
  status = "",
  status_is_error = false,
  buffers = {},
  windows = {},
  current_window = nil,
  modes = {},
}

function register_mode(mode)
  editor.modes[mode.name] = mode
end

function set_status(message, is_error)
  editor.status = message
  editor.status_is_error = is_error or false
end

function current_window()
  return editor.current_window
end

function current_buffer()
  return editor.current_window.buffer
end

function render()
  local b = "\x1b[?25l\x1b[H" -- Hide cursor, go home
  local winx = 0
  local winy = 0
  local buffer = editor.current_window.buffer
  local p = #tostring(#buffer.contents)
  for i, l in ipairs(buffer.contents) do
    b = b .. "\x1b[0K" .. padl(i, p, "0") .. " " .. l .. "\r\n"
  end
  local i = #buffer.contents
  while i < editor.height - 2 do
    b = b .. "\x1b[0K" .. padl("~", p, " ") .. "\r\n"
    i = i + 1
  end
  local modename = buffer.mode
  local minimodenames = map(buffer.minimodes, function(m)
    return m.name
  end)
  if #minimodenames > 0 then
    modename = modename .. "+" .. table.concat(minimodenames, ",")
  end
  local left = string.format(" (%s) %s (%d)",
    modename, buffer.name:filename(), #buffer.contents)
  local right = string.format("%d, %d ", buffer.x, buffer.y)
  local middle = string.rep(" ", editor.width - #left - #right)
  b = b .. "\x1b[0K\x1b[7m" .. left .. middle .. right .. "\x1b[0m\r\n"
  local status = editor.status .. " " ..
    padl(editor.keys, editor.width-#editor.status-1, " ")
  b = b .. "\x1b[0K" .. string.sub(status, 0, editor.width)
  b = b .. string.format("\x1b[%d;%dH\x1b[?25h", buffer.y+1, p+2+buffer.x)
  io.stdout:write(b)
  io.stdout:flush()
end

-- Buffer
--------------------------------------------------------

function buffer_insert(buffer, s)
  local lines = s:split("\n")
  local line = buffer.contents[buffer.y+1];
  lines[#lines] = lines[#lines]..string.sub(line, buffer.x+1)
  buffer.contents[buffer.y+1] = line:sub(0, buffer.x) .. lines[1]
  for i = #lines, 2, -1 do
    table.insert(buffer.contents, buffer.y+2, lines[i])
  end
  buffer_move(buffer, 0, 0)
end

function buffer_delete(buffer, count, x, y)
  x = x or buffer.x
  y = y or buffer.y
  local line = buffer.contents[y+1];

  -- HACK
  if count == -1 and x == 0 and y > 0 then
    buffer.contents[y] = buffer.contents[y] .. line
    table.remove(buffer.contents, y+1)
    buffer_move(buffer, 0, 0)
    return
  end

  local before = line:sub(0, math.max(0, x+math.min(0, count)))
  local after = line:sub(x+1+math.max(0, count))
  buffer.contents[y+1] = before .. after
  buffer_move(buffer, 0, 0)
  --return line:sub(x+math.min(0, count),
  --  buffer.x+math.max(0, count))
end

function buffer_move_to(buffer, x, y)
  buffer.y = math.min(#buffer.contents-1, math.max(0, y))
  buffer.x = math.min(utf8.len(buffer.contents[buffer.y+1]), math.max(0, x))
end

function buffer_move(buffer, x, y)
  buffer_move_to(buffer, buffer.x+x, buffer.y+y)
end

function buffer_line(buffer, y)
  y = y or buffer.y
  return buffer.contents[y+1]
end

function buffer_read(buffer)
  local ok, err = pcall(function()
    buffer.contents = {}
    for l in io.lines(buffer.name) do
      table.insert(buffer.contents, l)
    end
  end)
  if not ok then
    set_status(fmt("error loading file: %s: %s", path, err), true)
  end
  return ok
end

function new_buffer(name)
  return {
    name = name,
    x = 0,
    y = 0,
    xoff = 0,
    yoff = 0,
    edited = false,
    contents = {""},
    mode = "normal",
    minimodes = {},

    insert = buffer_insert,
    delete = buffer_delete,
    move = buffer_move,
    move_to = buffer_move_to,
    line = buffer_line,
    read = buffer_read,
  }
end

function new_buffer_from_file(path)
  local buffer = new_buffer(path)
  buffer:read()
  return buffer
end


-- Window
--------------------------------------------------------

function new_window(buffer)
  return {
    buffer = buffer
  }
end


-- Mode
--------------------------------------------------------

function mode_add(mode, key, handler)
  table.insert(mode.handlers, {key = key, handler = handler })
end

function mode_handle(mode, key)
  for _, def in ipairs(mode.handlers) do
    if string.ends(key, def.key) then
      def.handler(def.key)
      return true
    end
    if def.key == "any" then
      def.handler(key)
      return true
    end
  end
  return false
end

function new_mode(name)
  return {
    name = name,
    handlers = {},
    add = mode_add,
    handle = mode_handle,
  }
end

function handle_key(key)
  if editor.keys == "" then
    editor.keys = key
  else
    editor.keys = editor.keys .. " " .. key
  end
  local buffer = editor.current_window.buffer
  for i = #buffer.minimodes, 1, -1 do
    local mode = editor.modes[buffer.minimodes[i].name]
    if mode:handle(editor.keys) then
      editor.keys = ""
      return
    end
  end
  local mode = editor.modes[buffer.mode]
  if mode:handle(editor.keys) then
    editor.keys = ""
  end
end

-- Define modes
--------------------------------------------------------

normal_mode = new_mode("normal")
normal_mode:add("i", function()
  current_buffer().mode = "insert"
end)
normal_mode:add("A", function()
  local buffer = current_buffer()
  buffer:move(1000000, 0)
  buffer.mode = "insert"
end)
normal_mode:add("o", function()
  local buffer = current_buffer()
  buffer:move(1000000, 0)
  buffer:insert('\n')
  buffer:move(0, 1)
  buffer.mode = "insert"
end)
normal_mode:add("esc", function()
  editor.keys = ""
end)
normal_mode:add("h", function()
  current_buffer():move(-1, 0)
end)
normal_mode:add("l", function()
  current_buffer():move(1, 0)
end)
normal_mode:add("j", function()
  current_buffer():move(0, 1)
end)
normal_mode:add("k", function()
  current_buffer():move(0, -1)
end)
normal_mode:add("0", function()
  local buffer = current_buffer()
  buffer:move_to(-1000000, buffer.y)
end)
normal_mode:add("$", function()
  local buffer = current_buffer()
  buffer:move_to(1000000, buffer.y)
end)
normal_mode:add("g g", function()
  local buffer = current_buffer()
  buffer:move_to(buffer.x, 0)
end)
normal_mode:add("G", function()
  local buffer = current_buffer()
  buffer:move_to(buffer.x, 1000000)
end)
normal_mode:add("ctrl-d", function()
  current_buffer():move(0, 15)
end)
normal_mode:add("ctrl-u", function()
  current_buffer():move(0, -15)
end)
register_mode(normal_mode)

insert_mode = new_mode("insert")
insert_mode:add("esc", function()
  current_buffer().mode = "normal"
end)
insert_mode:add("ctrl-c", function()
  current_buffer().mode = "normal"
end)
insert_mode:add("backspace", function(char)
  local buffer = current_buffer()
  local x, y = buffer.x, buffer.y
  if buffer.x == 0 and buffer.y > 0 then
    buffer:move(1000000, -1)
  else
    buffer:move(-1, 0)
  end
  buffer:delete(-1, x, y)
end)
insert_mode:add("del", function(char)
  local buffer = current_buffer()
  buffer:delete(1)
end)
insert_mode:add("enter", function(char)
  local buffer = current_buffer()
  local line = buffer:line()
  buffer:insert("\n")
  buffer:move(-1000000, 1)
  local spaces = line:match("^( *).*")
  buffer:insert(spaces)
  buffer:move(#spaces, 0)
end)
insert_mode:add("tab", function(char)
  local buffer = current_buffer()
  buffer:insert("  ")
  buffer:move(2, 0)
end)
insert_mode:add("any", function(char)
  if #char > 1 then return end
  local buffer = current_buffer()
  buffer:insert(char)
  buffer:move(1, 0)
end)
register_mode(insert_mode)


-- Initialization
--------------------------------------------------------

function run()
  if #args > 1 then
    table.remove(args, 1)
    for _, b in ipairs(args) do
      table.insert(editor.buffers, new_buffer_from_file(b))
    end
  else
    table.insert(editor.buffers, new_buffer("*scratch*"))
  end
  table.insert(editor.windows, new_window(editor.buffers[1]))
  editor.current_window = editor.windows[1]
  while true do
    w, h = screen_size()
    editor.width = w
    editor.height = h
    render()
    handle_key(next_keypress())
  end
end
